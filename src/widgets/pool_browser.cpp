#include "pool_browser.hpp"
#include "pool/pool.hpp"
#include <set>
#include "util/gtk_util.hpp"
#include "cell_renderer_color_box.hpp"
#include "pool/pool_manager.hpp"
#include "tag_entry.hpp"

namespace horizon {
PoolBrowser::PoolBrowser(Pool *p) : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0), pool(p)
{
    const auto &pools = PoolManager::get().get_pools();
    if (pools.count(pool->get_base_path()))
        pool_uuid = pools.at(pool->get_base_path()).uuid;
}

Gtk::Entry *PoolBrowser::create_search_entry(const std::string &label)
{
    auto entry = Gtk::manage(new Gtk::SearchEntry());
    add_search_widget(label, *entry);
    entry->signal_search_changed().connect(sigc::mem_fun(*this, &PoolBrowser::search));
    search_entries.insert(entry);
    return entry;
}

TagEntry *PoolBrowser::create_tag_entry(const std::string &label)
{
    auto entry = Gtk::manage(new TagEntry(*pool, get_type()));
    add_search_widget(label, *entry);
    entry->signal_changed().connect(sigc::mem_fun(*this, &PoolBrowser::search));
    tag_entries.insert(entry);
    return entry;
}

void PoolBrowser::add_search_widget(const std::string &label, Gtk::Widget &w)
{
    auto la = Gtk::manage(new Gtk::Label(label));
    la->get_style_context()->add_class("dim-label");
    la->set_halign(Gtk::ALIGN_END);
    grid->attach(*la, 0, grid_top, 1, 1);
    w.set_hexpand(true);
    grid->attach(w, 1, grid_top, 1, 1);
    grid_top++;
    la->show();
    w.show();
}

Gtk::TreeViewColumn *PoolBrowser::append_column(const std::string &name, const Gtk::TreeModelColumnBase &column,
                                                Pango::EllipsizeMode ellipsize)

{
    auto cr = Gtk::manage(new Gtk::CellRendererText());
    auto tvc = Gtk::manage(new Gtk::TreeViewColumn(name, *cr));
    tvc->add_attribute(cr->property_text(), column);
    cr->property_ellipsize() = ellipsize;
    treeview->append_column(*tvc);
    return tvc;
}
Gtk::TreeViewColumn *PoolBrowser::append_column_with_item_source_cr(const std::string &name,
                                                                    const Gtk::TreeModelColumnBase &column,
                                                                    Pango::EllipsizeMode ellipsize)

{
    auto tvc = Gtk::manage(new Gtk::TreeViewColumn(name));
    auto cr = Gtk::manage(new Gtk::CellRendererText());
    if (pool_uuid) {
        auto cr_cb = create_pool_item_source_cr(tvc);
        tvc->pack_start(*cr_cb, false);
    }
    tvc->pack_start(*cr, true);
    tvc->add_attribute(cr->property_text(), column);
    cr->property_ellipsize() = ellipsize;
    treeview->append_column(*tvc);
    return tvc;
}

void PoolBrowser::construct(Gtk::Widget *search_box)
{

    store = create_list_store();

    if (search_box) {
        search_box->show();
        pack_start(*search_box, false, false, 0);
    }
    else {
        grid = Gtk::manage(new Gtk::Grid());
        grid->set_column_spacing(10);
        grid->set_row_spacing(10);

        grid->set_margin_top(20);
        grid->set_margin_start(20);
        grid->set_margin_end(20);
        grid->set_margin_bottom(20);

        grid->show();
        pack_start(*grid, false, false, 0);
    }

    auto sep = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
    sep->show();
    pack_start(*sep, false, false, 0);

    treeview = Gtk::manage(new Gtk::TreeView(store));

    auto sc = Gtk::manage(new Gtk::ScrolledWindow());
    sc->add(*treeview);
    sc->set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
    sc->show();

    pack_start(*sc, true, true, 0);
    treeview->show();

    create_columns();
    treeview->get_selection()->set_mode(Gtk::SelectionMode::SELECTION_BROWSE);

    sort_controller = std::make_unique<SortController>(treeview);
    sort_controller->set_simple(true);
    add_sort_controller_columns();
    sort_controller->set_sort(0, SortController::Sort::ASC);
    sort_controller->signal_changed().connect(sigc::mem_fun(*this, &PoolBrowser::search));

    // dynamic_cast<Gtk::CellRendererText*>(view->get_column_cell_renderer(3))->property_ellipsize()
    // = Pango::ELLIPSIZE_END;
    treeview->get_selection()->set_mode(Gtk::SelectionMode::SELECTION_BROWSE);
    treeview->signal_row_activated().connect(sigc::mem_fun(*this, &PoolBrowser::row_activated));
    treeview->get_selection()->signal_changed().connect(sigc::mem_fun(*this, &PoolBrowser::selection_changed));
    if (path_column) {
        path_column->set_visible(false);
    }
    treeview->signal_button_press_event().connect_notify([this](GdkEventButton *ev) {
        Gtk::TreeModel::Path path;
        if (ev->button == 3 && context_menu.get_children().size()) {
#if GTK_CHECK_VERSION(3, 22, 0)
            context_menu.popup_at_pointer((GdkEvent *)ev);
#else
            context_menu.popup(ev->button, gtk_get_current_event_time());
#endif
        }
    });
}

void PoolBrowser::row_activated(const Gtk::TreeModel::Path &path, Gtk::TreeViewColumn *column)
{
    auto it = store->get_iter(path);
    if (it) {
        s_signal_activated.emit();
    }
}

void PoolBrowser::selection_changed()
{
    s_signal_selected.emit();
}

UUID PoolBrowser::get_selected()
{
    auto it = treeview->get_selection()->get_selected();
    if (it) {
        Gtk::TreeModel::Row row = *it;
        return uuid_from_row(row);
    }
    return UUID();
}

bool PoolBrowser::get_any_selected()
{
    return treeview->get_selection()->count_selected_rows();
}

void PoolBrowser::set_show_none(bool v)
{
    show_none = v;
    search();
}

void PoolBrowser::set_show_path(bool v)
{
    show_path = v;
    if (path_column) {
        path_column->set_visible(v);
    }
    search();
}

void PoolBrowser::scroll_to_selection()
{
    tree_view_scroll_to_selection(treeview);
}

void PoolBrowser::select_uuid(const UUID &uu)
{
    for (const auto &it : store->children()) {
        if (uuid_from_row(*it) == uu) {
            treeview->get_selection()->select(it);
            break;
        }
    }
}

void PoolBrowser::clear_search()
{
    for (auto it : search_entries) {
        it->set_text("");
    }
    for (auto it : tag_entries) {
        it->clear();
    }
}

void PoolBrowser::go_to(const UUID &uu)
{
    clear_search();
    select_uuid(uu);
    scroll_to_selection();
}

void PoolBrowser::add_context_menu_item(const std::string &label, sigc::slot1<void, UUID> cb)
{
    auto la = Gtk::manage(new Gtk::MenuItem(label));
    la->show();
    context_menu.append(*la);
    la->signal_activate().connect([this, cb] { cb(get_selected()); });
}

PoolBrowser::PoolItemSource PoolBrowser::pool_item_source_from_db(const UUID &uu, bool overridden)
{
    if (overridden && uu == pool_uuid)
        return PoolItemSource::OVERRIDING;
    else if (uu == pool_uuid)
        return PoolItemSource::LOCAL;
    else
        return PoolItemSource::INCLUDED;
}

void PoolBrowser::install_pool_item_source_tooltip()
{
    treeview->set_has_tooltip(true);
    treeview->signal_query_tooltip().connect(
            [this](int x, int y, bool keyboard_tooltip, const Glib::RefPtr<Gtk::Tooltip> &tooltip) {
                if (keyboard_tooltip)
                    return false;
                Gtk::TreeModel::Path path;
                Gtk::TreeViewColumn *column;
                int cell_x, cell_y;
                int bx, by;
                treeview->convert_widget_to_bin_window_coords(x, y, bx, by);
                if (!treeview->get_path_at_pos(bx, by, path, column, cell_x, cell_y))
                    return false;
                Gtk::TreeIter iter(treeview->get_model()->get_iter(path));
                if (!iter)
                    return false;
                PoolItemSource src = pool_item_source_from_row(*iter);
                switch (src) {
                case PoolItemSource::LOCAL:
                    tooltip->set_text("Item is from this pool");
                    break;
                case PoolItemSource::INCLUDED:
                    tooltip->set_text("Item is from included pool");
                    break;
                case PoolItemSource::OVERRIDING:
                    tooltip->set_text("Item is from this pool overriding an item from an included pool");
                    break;
                }
                //
                treeview->set_tooltip_row(tooltip, path);
                return true;
            });
}

PoolBrowser::PoolItemSource PoolBrowser::pool_item_source_from_row(const Gtk::TreeModel::Row &row)
{
    return PoolItemSource::LOCAL;
}

CellRendererColorBox *PoolBrowser::create_pool_item_source_cr(Gtk::TreeViewColumn *tvc)
{
    auto cr_cb = Gtk::manage(new CellRendererColorBox());
    tvc->set_cell_data_func(*cr_cb, [this](Gtk::CellRenderer *tcr, const Gtk::TreeModel::iterator &it) {
        auto mcr = dynamic_cast<CellRendererColorBox *>(tcr);
        Color co(1, 0, 1);
        switch (pool_item_source_from_row(*it)) {
        case PoolItemSource::LOCAL:
            co = Color::new_from_int(138, 226, 52);
            break;

        case PoolItemSource::INCLUDED:
            co = Color::new_from_int(252, 175, 62);
            break;

        default:
            co = Color::new_from_int(239, 41, 41);
        }
        Gdk::RGBA va;
        va.set_red(co.r);
        va.set_green(co.g);
        va.set_blue(co.b);
        mcr->property_color() = va;
    });
    return cr_cb;
}
} // namespace horizon
