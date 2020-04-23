#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "scantools.h"

#include <QMainWindow>
#include <QThread>
#include <QTreeWidgetItem>
#include <QBrush>
#include <QAction>
#include <memory>
#include <queue>

namespace Ui {
    class MainWindow;
}

struct console {

    const size_t MAX_NUMBER = 12;
private:
    size_t number = 0;
    std::deque<QString> lines;

public:
    void add_line(QString &line) {
        if (number < MAX_NUMBER) {
            number++;
        } else {
            lines.pop_front();
        }
        lines.push_back(line);
    }
    QString get_text() {
        QString output = "";
        for (size_t i = 0; i < number; i++) {
            QString line = lines.front();
            lines.pop_front();
            output += "<br/>" + line;
            lines.push_back(line);
        }
        return output;
    }
    void change_line(QString &line) {
        if (number == 0) add_line(line);
        lines.pop_back();
        lines.push_back(line);
    }
};

class main_window : public QMainWindow {
    Q_OBJECT

private slots:
    /* slots from ui signals */
    void about_slot();
    void again_slot();
    void back_slot();
    void choose_slot();
    void find_substring_slot();
    void exit_slot();
    void open_slot(QTreeWidgetItem *item, int column);
    void refresh_slot();
    void scan_slot();

    void error(QString &text);

    /* slots from scantools signals */
    void started_slot();
    void finished_slot();
    void console_slot(QString text, bool save, QString color = "");
    void add_item(QString s1 = "", QString s2 = "", QString s3 = "", QString s4 = "", QString s5 = "", QBrush *brush = nullptr);
    void color_item(QTreeWidgetItem *item, QBrush *brush);
    void clear_items();
    void update_items();

private:
    QThread *thread;
    std::unique_ptr<Ui::MainWindow> ui;
    scantools st;
    console cs;
    bool scanning = false;

    int short_dialog(QString const &text);
    int long_dialog(QString const &text, QString const &information);
    std::pair<bool, QString> input_dialog();

    void setItemsEnabled(bool f, QAction *x) { x->setEnabled(f); }
    void setItemsVisible(bool f, QAction *x) { x->setVisible(f); }
    template<class... Args>
    void setItemsEnabled(bool f, QAction *x, Args &...args) {
        setItemsEnabled(f, x);
        setItemsEnabled(f, args...);
    }
    template<class... Args>
    void setItemsVisible(bool f, QAction *x, Args &...args) {
        setItemsVisible(f, x);
        setItemsVisible(f, args...);
    }
    void setText(QAction *x, QString text) {
        x->setText(text);
        x->setIconText(text);
        x->setToolTip(text);
    }

    void setHeaderSpecialText(QTreeWidget *treeWidget) {
        treeWidget->headerItem()->setText(0, "Path");
        treeWidget->headerItem()->setText(1, "Position");
        treeWidget->headerItem()->setText(2, "");
        treeWidget->headerItem()->setText(3, "");
        treeWidget->headerItem()->setText(4, "");
    }

    void setHeaderDefaultText(QTreeWidget *treeWidget) {
        treeWidget->headerItem()->setText(0, "Name ");
        treeWidget->headerItem()->setText(1, "Type");
        treeWidget->headerItem()->setText(2, "Size");
        treeWidget->headerItem()->setText(3, "Path");
        treeWidget->headerItem()->setText(4, "Modified");
    }

public:
    explicit main_window(QWidget *parent = nullptr);
    ~main_window();
};

#endif // MAINWINDOW_H
