#ifndef SCANTOOLS_H
#define SCANTOOLS_H

#include <QFileInfo>
#include <QDebug>
#include <QString>
#include <queue>
#include <vector>
#include <set>
#include <unordered_set>
#include <QTreeWidget>
#include <QDateTime>
#include <QDir>
#include <QFileSystemWatcher>

#include <memory>

static QColor red = QColor(255, 0, 0);
static QColor pure_blue = QColor(0, 136, 255);
static QColor black = QColor(0, 0, 0);
static QBrush red_brush = QBrush(red);
static QBrush pure_blue_brush = QBrush(pure_blue);
static QBrush black_brush = QBrush(black);

struct file {
    QString name;
    QString path;
    long long size;
    QDateTime date;

    bool skip;
    bool duplicated;
    size_t first;
    QString hash;

    file(QFileInfo &file_info) {
        name = file_info.fileName();
        path = file_info.absoluteFilePath();
        size = file_info.size();
        date = file_info.lastModified();
        skip = !file_info.isReadable();
        duplicated = false;
        hash = "";
    }
    bool compare(file &another) {
        return size == another.size && hash == another.hash;
    }
};

class scantools : public QObject {
    Q_OBJECT

signals:
    void started();
    void paused();
    void canceled();
    void finished();
    void console(QString text, bool save, QString color = "");
    void add_item(QString s1 = "", QString s2 = "", QString s3 = "", QString s4 = "", QString s5 = "", QBrush *brush = nullptr);
    void color_item(QTreeWidgetItem *item, QBrush *brush);
    void clear_items();
    void update_items();

public slots:
    void start();
    void end();

    void check_file(const QString &path);

private:
    /* we can use it in any part of code */
    bool mode;
    size_t result;
    QString main_directory;
    enum {SCAN_DIRS, INDEX_FILES, WATCH_FILES, END} scanning_state;
    enum {PREPARED, SCANNING, PAUSED, CANCELED, FINISHED} main_state;

    /* we can use it only in while-switch block in start */
    std::vector<file> files;
    std::queue<QString> dirs;

    QFileSystemWatcher file_watcher;
    std::map<std::string, std::unique_ptr<std::set<std::string>>> index;

    std::set<std::string> indexed;

    /* parts of scanning */
    void scan_directories();
    void sort_by_size();
    void calculate_hashes(size_t, size_t);
    void sort_by_hash(size_t);
    void group_duplicates(size_t, size_t);
    void sort_by_name(size_t);
    void show_results(size_t i0, size_t j0);

    void do_smth();
    void index_files();

    /* service */
    void check(size_t i = 0, size_t j = 0);
    void clear();

public:
    /* standard methods */
    scantools(bool mode = false);
    ~scantools() {
        clear();
        end();
    }

    /* reserved methods */
    void pause() {  }
    void cancel() {  }

    /* changing methods */
    void clean();

    void find_substring(QString substring);
    void open_directory(QString path = QDir::currentPath());

    /* describing methods */
    bool is_prepared() { return main_state == PREPARED; }
    bool is_scanning() { return main_state == SCANNING; }
    bool is_paused() { return main_state == PAUSED; }
    bool is_canceled() { return main_state == CANCELED; }
    bool is_finished() { return main_state == FINISHED; }
    size_t number_for_deleting() { return (is_finished()) ? result : 0; }
};

#endif // SCANTOOLS_H
