#include "scantools.h"
#include "utf8_validator.hpp"
#include "KnuthMorrisPrattAlgorithm.cpp"

#include <QDir>
#include <QDebug>
#include <algorithm>
#include <QCryptographicHash>
#include <QFileInfoList>
#include <QDateTime>
#include <QThread>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>

const size_t CHECK_SIZE = 20000;
const size_t BLOCK_SIZE = 1024 * 1024;
const size_t GRAM_SIZE = 3;
const QString FORMAT = "d MMMM yyyy, hh:mm:ss";

scantools::scantools(bool mode) : mode(mode) {
    result = 0;
    main_state = PREPARED;
    QDir::setCurrent(QDir::homePath());
    scanning_state = SCAN_DIRS;
}

void scantools::start() {
    main_state = SCANNING;
    emit started();
    while (scanning_state != END) {
        switch (scanning_state) {
            case SCAN_DIRS: scan_directories(); break;
            case INDEX_FILES: index_files(); break;
            default: scanning_state = END;
        }
    }
    emit console("FINISHED", true, "green");
    main_state = FINISHED;
    clear();
    emit finished();
    open_directory();
}

void scantools::end() {
    disconnect(&file_watcher, &QFileSystemWatcher::fileChanged, this, &scantools::check_file);
    for (auto it = index.begin(); it != index.end(); it++) {
        file_watcher.removePath(QString::fromStdString(it->first));
    }
    index.clear();
    indexed.clear();
}

void scantools::clear() {
    main_state = PREPARED;
    scanning_state = SCAN_DIRS;
    files.clear();
    while (!dirs.empty()) dirs.pop();
    emit console("", true);
}

void scantools::scan_directories() {
    emit console("scanning directories..", true);
    if (dirs.empty()) dirs.push(main_directory);
    while (!dirs.empty()) {
        emit console(QString("scanning ").append(dirs.front()).append(" (0%)"), false);
        QDir d(dirs.front());
        QFileInfoList list = d.entryInfoList(QDir::NoDotAndDotDot | QDir::NoSymLinks | QDir::Dirs | QDir::Files);
        for (size_t i = 0; i < list.size(); i++) {
            emit console(QString("scanning ").append(dirs.front()).append(" (%1%)").arg((i + 1) * 100 / list.size()), false);
            if (list[i].isDir()) dirs.push(list[i].filePath());
            if (list[i].isFile()) {
                files.emplace_back(list[i]);
            }
        }
        dirs.pop();
    }
    emit console("scanning directories..", false);
    scanning_state = INDEX_FILES;
}

void scantools::index_files() {
    emit console("indexing files..", true);
    std::vector<QFuture<void>> v;
    size_t number_of_files = 0;
    for (size_t i = 0; i < files.size(); i++) {
        if (index.count(files[i].path.toStdString()) > 0) {
            continue;
        }
        index.insert({files[i].path.toStdString(), std::unique_ptr<std::set<std::string>>(new std::set<std::string>())}).second;
        v.push_back(QtConcurrent::run([&] (const QString& s) {
            auto g = index.find(s.toStdString());

            emit console(QString("indexing files (%1%) ..").arg((number_of_files + 1) * 100 / files.size()), false);
            number_of_files++;
            QFile file(s);
            if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
                index.erase(g);
                return;
            }
            std::queue<char> symbols;
            std::vector<char> buffer(BLOCK_SIZE);
            std::uint32_t state = UTF8_ACCEPT;
            while (!file.atEnd()) {
                qint64 count_read = file.read(&buffer[0], BLOCK_SIZE);
                if (count_read == -1 || validate_utf8(&state, &buffer[0], buffer.size()) == UTF8_REJECT) {
                    file.close();
                    index.erase(g);
                    return;
                }
                for (size_t k = 0; k < buffer.size(); k++) {
                    symbols.push(buffer.at(k));
                    if (symbols.size() < GRAM_SIZE) continue;
                    if (symbols.size() > GRAM_SIZE) symbols.pop();
                    std::string ngram = "";
                    for (size_t j = 0; j < GRAM_SIZE; j++) {
                        ngram += symbols.front();
                        symbols.push(symbols.front());
                        symbols.pop();
                    }
                    g->second->insert(ngram);
                }
            }
            file.close();
            if (g->second->size() > CHECK_SIZE) {
                index.erase(g);
            }
            return;
        }, std::cref(files[i].path)));
    }
    for (auto it = v.begin(); it != v.end(); it++) {
        it->waitForFinished();
    }
    emit console("indexing files..", false);
    for (auto it = index.begin(); it != index.end(); it++) {
        indexed.insert(it->first);
        file_watcher.addPath(QString::fromStdString((*it).first));
    }
    connect(&file_watcher, &QFileSystemWatcher::fileChanged, this, &scantools::check_file);
    scanning_state = END;
}

void scantools::check_file(const QString &path) {
    if (index.count(path.toStdString()) == 0) {
        return;
    }
    auto g = index.find(path.toStdString());
    g->second->clear();
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        index.erase(g);
        indexed.erase(path.toStdString());
        return;
    }
    std::queue<char> symbols;
    std::vector<char> buffer(BLOCK_SIZE);
    std::uint32_t state = UTF8_ACCEPT;
    while (!file.atEnd()) {
        qint64 count_read = file.read(&buffer[0], BLOCK_SIZE);
        if (count_read == -1 || validate_utf8(&state, &buffer[0], buffer.size()) == UTF8_REJECT) {
            file.close();
            index.erase(g);
            indexed.erase(path.toStdString());
            return;
        }
        for (size_t k = 0; k < buffer.size(); k++) {
            symbols.push(buffer.at(k));
            if (symbols.size() < GRAM_SIZE) continue;
            if (symbols.size() > GRAM_SIZE) symbols.pop();
            std::string ngram = "";
            for (size_t j = 0; j < GRAM_SIZE; j++) {
                ngram += symbols.front();
                symbols.push(symbols.front());
                symbols.pop();
            }
            g->second->insert(ngram);
        }
    }
    file.close();

    if (g->second->size() <= CHECK_SIZE) {
        console(QString("File was changed: ").append(path), true, "pink");
    } else {
        console(QString("File was removed: ").append(path), true, "pink");
        index.erase(g);
        indexed.erase(path.toStdString());
        file_watcher.removePath(path);
    }
}

void scantools::find_substring(QString substring) {
    emit clear_items();
    std::string s = substring.toStdString();
    std::vector<std::string> ngrams;
    if (s.length() >= GRAM_SIZE) {
        for (size_t i = 0; i < s.length() - GRAM_SIZE + 1; i++) {
            ngrams.push_back(s.substr(i, GRAM_SIZE));
        }
    }
    std::vector<QFuture<std::pair<std::string, std::pair<bool, qint64>>>> v;
    for (auto it = index.begin(); it != index.end(); it++) {
        v.push_back(QtConcurrent::run([&] (const std::string& s,  const std::unique_ptr<std::set<std::string>> &p) {
                        bool f = true;
                        for (size_t i = 0; i < ngrams.size(); i++) {
                            if (p->count(ngrams[i]) == 0) {
                                f = false;
                                break;
                            }
                        }
                        if (f) {
                            QFile file(QString::fromStdString(s));
                            file.open(QIODevice::ReadOnly);
                            std::vector<char> buffer(BLOCK_SIZE);

                            size_t block_size = BLOCK_SIZE, substring_size = 0;
                            while (!file.atEnd()) {
                                qint64 count_read = file.read(&buffer[substring_size], block_size);
                                if (count_read == -1 ) break;
                                if (block_size == BLOCK_SIZE) {
                                    block_size -= substring.length();
                                    substring_size += substring.length();
                                }
                                std::string str(buffer.begin(), buffer.end());
                                qint64 x = KMPAlgorithm(str, substring.toStdString());
                                if (x != -1) {
                                    return std::make_pair(s, std::make_pair(true, x));
                                }
                                if (!file.atEnd()) {
                                    for (size_t i = 0; i < substring_size; i++) {
                                        buffer[i] = buffer[block_size + i];
                                    }
                                }
                            }
                        }
                        return std::make_pair(s, std::make_pair(false, 0ll));
        }, std::cref(it->first), std::cref(it->second)));
    }
    for (auto it = v.begin(); it != v.end(); it++) {
        auto result = it->result();
        if (result.second.first) {
            emit add_item(QString::fromStdString(result.first), QString::number(result.second.second));
        }
    }
    emit update_items();
}

void scantools::open_directory(QString path) {
    QFileInfo file_info(path);
    if (!file_info.isDir()) {
        emit console(path.append(" is not a directory"), true, "blue");
    } else if (!file_info.isReadable()) {
        emit console(QString("cannot open ").append(path), true, "blue");
    } else {
        QDir::setCurrent(path);
        emit clear_items();
        QFileInfoList list = QDir::current().entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files);
        std::stable_sort(list.begin(), list.end(), [](const QFileInfo &f1, const QFileInfo &f2) {
            return (f1.isDir() && f2.isFile()) || ((f1.isDir() || f1.isFile()) && (!f2.isDir() && !f2.isFile()));
        });
        if (!QDir::current().isRoot()) {
            emit add_item("..");
        }
        for (QFileInfo f: list) {
            if (index.count(f.absoluteFilePath().toStdString()) > 0) {
                emit add_item(f.fileName(), (f.isDir()) ? "directory" : (f.isFile()) ? "file" : "", (f.isFile()) ? QString::number(f.size()) : "", f.absoluteFilePath(), f.lastModified().toString(FORMAT), &pure_blue_brush);
            } else {
                emit add_item(f.fileName(), (f.isDir()) ? "directory" : (f.isFile()) ? "file" : "", (f.isFile()) ? QString::number(f.size()) : "", f.absoluteFilePath(), f.lastModified().toString(FORMAT));
            }
        }
        emit update_items();
        emit console(QString("open ").append(QDir::currentPath()), true, "blue");
    }
}
