#include "filebrowser.h"
#include "../src/core/map_builder.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <fstream>
#include <direct.h>

static void mkdir_recursive(const std::string& path) {
    std::string cur;
    for (size_t i = 0; i < path.size(); ++i) {
        char c = path[i];
        if (c == '/' || c == '\\') {
            _mkdir(cur.c_str());
        }
        cur += c;
    }
    _mkdir(cur.c_str());
}

FileBrowser::FileBrowser(QWidget* parent)
    : QWidget(parent)
{
    build_ui();
}

void FileBrowser::build_ui() {
    auto* layout = new QVBoxLayout(this);

    tree_ = new QTreeWidget(this);
    tree_->setHeaderLabels({tr("Name"), tr("Size")});
    tree_->header()->setStretchLastSection(false);
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree_->setAlternatingRowColors(true);
    tree_->setRootIsDecorated(true);

    auto* btn_row = new QHBoxLayout;
    extract_btn_ = new QPushButton(tr("Extract Selected"));
    extract_all_btn_ = new QPushButton(tr("Extract All"));
    replace_btn_ = new QPushButton(tr("Replace File..."));
    remove_btn_ = new QPushButton(tr("Remove"));

    connect(extract_btn_, &QPushButton::clicked, this, &FileBrowser::on_extract_selected);
    connect(extract_all_btn_, &QPushButton::clicked, this, &FileBrowser::on_extract_all);
    connect(replace_btn_, &QPushButton::clicked, this, &FileBrowser::on_replace);
    connect(remove_btn_, &QPushButton::clicked, this, &FileBrowser::on_remove);

    btn_row->addStretch();
    btn_row->addWidget(extract_btn_);
    btn_row->addWidget(extract_all_btn_);
    btn_row->addWidget(replace_btn_);
    btn_row->addWidget(remove_btn_);
    btn_row->addStretch();

    layout->addWidget(tree_);
    layout->addLayout(btn_row);
}

void FileBrowser::load(MapBuilder* builder) {
    builder_ = builder;
    clear();

    if (!builder_) return;
    auto files = builder_->list_files();
    populate_tree(files);
}

void FileBrowser::clear() {
    tree_->clear();
}

void FileBrowser::populate_tree(const std::vector<std::string>& files) {
    // Build path components for tree display
    struct Node {
        QTreeWidgetItem* item;
        std::string full_path;
    };

    std::map<std::string, Node> nodes;

    for (auto& f : files) {
        auto parts = f;
        std::string accumulated;
        QTreeWidgetItem* parent = nullptr;

        // Split path into components
        size_t start = 0;
        size_t end;
        while ((end = parts.find('/', start)) != std::string::npos) {
            auto segment = parts.substr(start, end - start);
            accumulated += (accumulated.empty() ? "" : "/") + segment;

            auto it = nodes.find(accumulated);
            if (it == nodes.end()) {
                auto* item = new QTreeWidgetItem;
                item->setText(0, QString::fromStdString(segment));
                item->setText(1, "");

                if (parent) {
                    parent->addChild(item);
                } else {
                    tree_->addTopLevelItem(item);
                }

                Node n;
                n.item = item;
                n.full_path = accumulated;
                nodes[accumulated] = n;
                parent = item;
            } else {
                parent = it->second.item;
            }
            start = end + 1;
        }

        // File leaf
        auto filename = parts.substr(start);
        accumulated += (accumulated.empty() ? "" : "/") + filename;

        auto* item = new QTreeWidgetItem;
        item->setText(0, QString::fromStdString(filename));

        auto sz = builder_->file_size(f);
        item->setText(1, QString::number(sz));
        item->setData(0, Qt::UserRole, QString::fromStdString(f));

        if (parent) {
            parent->addChild(item);
        } else {
            tree_->addTopLevelItem(item);
        }
    }

    tree_->expandAll();
}

void FileBrowser::on_extract_selected() {
    if (!builder_) return;

    auto items = tree_->selectedItems();
    if (items.isEmpty()) return;

    QString dir = QFileDialog::getExistingDirectory(this, tr("Extract to"));
    if (dir.isEmpty()) return;

    for (auto* item : items) {
        QString path = item->data(0, Qt::UserRole).toString();
        if (path.isEmpty()) continue; // directory node

        auto data = builder_->read_raw(path.toStdString());
        std::string out_path = dir.toLocal8Bit().toStdString() + "/" + path.toStdString();
        auto slash = out_path.find_last_of('/');
        if (slash != std::string::npos) {
            mkdir_recursive(out_path.substr(0, slash));
        }
        FILE* f = fopen(out_path.c_str(), "wb");
        if (f) {
            fwrite(data.data(), 1, data.size(), f);
            fclose(f);
        }
    }
}

void FileBrowser::on_extract_all() {
    if (!builder_) return;

    QString dir = QFileDialog::getExistingDirectory(this, tr("Extract All to"));
    if (dir.isEmpty()) return;

    auto files = builder_->list_files();
    for (auto& name : files) {
        auto data = builder_->read_raw(name);
        std::string out_path = dir.toLocal8Bit().toStdString() + "/" + name;
        auto slash = out_path.find_last_of('/');
        if (slash != std::string::npos) {
            mkdir_recursive(out_path.substr(0, slash));
        }
        FILE* f = fopen(out_path.c_str(), "wb");
        if (f) {
            fwrite(data.data(), 1, data.size(), f);
            fclose(f);
        }
    }
}

void FileBrowser::on_replace() {
    if (!builder_) return;

    auto items = tree_->selectedItems();
    if (items.isEmpty()) return;

    QString path = items.first()->data(0, Qt::UserRole).toString();
    if (path.isEmpty()) return;

    QString file_path = QFileDialog::getOpenFileName(this, tr("Replace: %1").arg(path));
    if (file_path.isEmpty()) return;

    std::ifstream is(file_path.toLocal8Bit().toStdString(), std::ios::binary | std::ios::ate);
    if (!is) return;

    std::streamsize size = is.tellg();
    is.seekg(0);
    std::vector<uint8_t> data(size);
    is.read((char*)data.data(), size);

    builder_->set_file(path.toStdString(), std::move(data));
    emit contentChanged();

    // Refresh tree
    load(builder_);
}

void FileBrowser::on_remove() {
    if (!builder_) return;

    auto items = tree_->selectedItems();
    if (items.isEmpty()) return;

    QString path = items.first()->data(0, Qt::UserRole).toString();
    if (path.isEmpty()) return;

    auto ret = QMessageBox::question(this, tr("Remove"),
        tr("Remove %1 from the archive?").arg(path));
    if (ret != QMessageBox::Yes) return;

    builder_->remove_file(path.toStdString());
    emit contentChanged();

    // Refresh tree
    load(builder_);
}

// ============================================================
// FileBrowserPlugin
// ============================================================
bool FileBrowserPlugin::init(PluginContext& ctx) {
    builder_ = ctx.builder;
    browser_ = new FileBrowser(ctx.parent_widget);
    connect(browser_, &FileBrowser::contentChanged, this, &FileBrowserPlugin::contentChanged);
    return true;
}

void FileBrowserPlugin::activate() {
    if (browser_ && builder_)
        browser_->load(builder_);
}

void FileBrowserPlugin::deactivate() {
    if (browser_) browser_->clear();
}

void FileBrowserPlugin::destroy() {
    delete browser_;
    browser_ = nullptr;
    builder_ = nullptr;
}
