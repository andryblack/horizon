#include "tool_helper_move.hpp"
#include "core_board.hpp"
#include "core_package.hpp"
#include "core_padstack.hpp"
#include "core_schematic.hpp"
#include "core_symbol.hpp"
#include "board/board_layers.hpp"

namespace horizon {
void ToolHelperMove::move_init(const Coordi &c)
{
    last = c;
    origin = c;
}

static Coordi *get_dim_coord(Dimension *dim, int vertex)
{
    if (vertex == 0)
        return &dim->p0;
    else if (vertex == 1)
        return &dim->p1;
    else
        return nullptr;
}

void ToolHelperMove::move_do(const Coordi &delta)
{
    std::set<UUID> no_label_distance;
    for (const auto &it : core.r->selection) {
        if (it.type == ObjectType::DIMENSION && it.vertex < 2) {
            no_label_distance.emplace(it.uuid);
        }
    }
    for (const auto &it : core.r->selection) {
        switch (it.type) {
        case ObjectType::JUNCTION:
            core.r->get_junction(it.uuid)->position += delta;
            break;
        case ObjectType::HOLE:
            core.r->get_hole(it.uuid)->placement.shift += delta;
            break;
        case ObjectType::SYMBOL_PIN:
            core.y->get_symbol_pin(it.uuid)->position += delta;
            break;
        case ObjectType::SCHEMATIC_SYMBOL:
            core.c->get_schematic_symbol(it.uuid)->placement.shift += delta;
            break;
        case ObjectType::BOARD_PACKAGE:
            core.b->get_board()->packages.at(it.uuid).placement.shift += delta;
            break;
        case ObjectType::PAD:
            core.k->get_package()->pads.at(it.uuid).placement.shift += delta;
            break;
        case ObjectType::BOARD_HOLE:
            core.b->get_board()->holes.at(it.uuid).placement.shift += delta;
            break;
        case ObjectType::TEXT:
            core.r->get_text(it.uuid)->placement.shift += delta;
            break;
        case ObjectType::POLYGON_VERTEX:
            core.r->get_polygon(it.uuid)->vertices.at(it.vertex).position += delta;
            break;
        case ObjectType::POLYGON_ARC_CENTER:
            core.r->get_polygon(it.uuid)->vertices.at(it.vertex).arc_center += delta;
            break;
        case ObjectType::SHAPE:
            core.a->get_padstack()->shapes.at(it.uuid).placement.shift += delta;
            break;
        case ObjectType::DIMENSION: {
            auto dim = core.r->get_dimension(it.uuid);
            if (it.vertex < 2) {
                Coordi *c = get_dim_coord(dim, it.vertex);
                *c += delta;
            }
            else if (no_label_distance.count(it.uuid) == 0) {
                dim->label_distance += dim->project(delta);
            }
        } break;
        default:;
        }
    }
}

void ToolHelperMove::move_do_cursor(const Coordi &cc)
{
    auto c = get_coord_restrict(origin, cc);
    auto delta = c - last;
    move_do(delta);
    last = c;
}


Coordi ToolHelperMove::get_delta() const
{
    return last - origin;
}


static void transform(Coordi &a, const Coordi &center, bool rotate)
{
    int64_t dx = a.x - center.x;
    int64_t dy = a.y - center.y;
    if (rotate) {
        a.x = center.x + dy;
        a.y = center.y - dx;
    }
    else {
        a.x = center.x - dx;
        a.y = center.y + dy;
    }
}

Orientation ToolHelperMove::transform_orientation(Orientation orientation, bool rotate, bool reverse)
{
    Orientation new_orientation = orientation;
    if (rotate) {
        if (!reverse) {
            switch (orientation) {
            case Orientation::UP:
                new_orientation = Orientation::RIGHT;
                break;
            case Orientation::DOWN:
                new_orientation = Orientation::LEFT;
                break;
            case Orientation::LEFT:
                new_orientation = Orientation::UP;
                break;
            case Orientation::RIGHT:
                new_orientation = Orientation::DOWN;
                break;
            }
        }
        else {
            switch (orientation) {
            case Orientation::UP:
                new_orientation = Orientation::LEFT;
                break;
            case Orientation::DOWN:
                new_orientation = Orientation::RIGHT;
                break;
            case Orientation::LEFT:
                new_orientation = Orientation::DOWN;
                break;
            case Orientation::RIGHT:
                new_orientation = Orientation::UP;
                break;
            }
        }
    }
    else {
        switch (orientation) {
        case Orientation::UP:
            new_orientation = Orientation::UP;
            break;
        case Orientation::DOWN:
            new_orientation = Orientation::DOWN;
            break;
        case Orientation::LEFT:
            new_orientation = Orientation::RIGHT;
            break;
        case Orientation::RIGHT:
            new_orientation = Orientation::LEFT;
            break;
        }
    }
    return new_orientation;
}

void ToolHelperMove::move_mirror_or_rotate(const Coordi &center, bool rotate)
{
    for (const auto &it : core.r->selection) {
        switch (it.type) {
        case ObjectType::JUNCTION:
            transform(core.r->get_junction(it.uuid)->position, center, rotate);
            break;
        case ObjectType::POLYGON_VERTEX:
            transform(core.r->get_polygon(it.uuid)->vertices.at(it.vertex).position, center, rotate);
            break;
        case ObjectType::DIMENSION:
            if (it.vertex < 2) {
                transform(*get_dim_coord(core.r->get_dimension(it.uuid), it.vertex), center, rotate);
            }
            else if (rotate == false) {
                auto dim = core.r->get_dimension(it.uuid);
                std::swap(dim->p0, dim->p1);
                dim->label_distance *= -1;
            }
            break;
        case ObjectType::POLYGON_ARC_CENTER:
            transform(core.r->get_polygon(it.uuid)->vertices.at(it.vertex).arc_center, center, rotate);
            if (!rotate) {
                core.r->get_polygon(it.uuid)->vertices.at(it.vertex).arc_reverse ^= 1;
            }
            break;

        case ObjectType::SYMBOL_PIN: {
            SymbolPin *pin = core.y->get_symbol_pin(it.uuid);
            transform(pin->position, center, rotate);
            pin->orientation = transform_orientation(pin->orientation, rotate);
        } break;

        case ObjectType::BUS_RIPPER: {
            auto &ri = core.c->get_sheet()->bus_rippers.at(it.uuid);
            if (rotate) {
                ri.orientation = transform_orientation(ri.orientation, true);
            }
            else {
                ri.mirror();
            }
        } break;

        case ObjectType::TEXT: {
            Text *txt = core.r->get_text(it.uuid);
            transform(txt->placement.shift, center, rotate);
            if (rotate) {
                if (txt->placement.mirror) {
                    txt->placement.inc_angle_deg(90);
                }
                else {
                    txt->placement.inc_angle_deg(-90);
                }
            }
            else {
                txt->placement.mirror = !txt->placement.mirror;
            }
        } break;

        case ObjectType::ARC:
            if (!rotate) {
                core.r->get_arc(it.uuid)->reverse();
            }
            break;
        case ObjectType::POWER_SYMBOL: {
            auto &ps = core.c->get_sheet()->power_symbols.at(it.uuid);
            if (rotate) {
                ps.orientation = transform_orientation(ps.orientation, true);
            }
            else {
                ps.mirrorx();
            }
        } break;

        case ObjectType::SCHEMATIC_SYMBOL: {
            SchematicSymbol *sym = core.c->get_schematic_symbol(it.uuid);
            transform(sym->placement.shift, center, rotate);
            if (rotate) {
                if (sym->placement.mirror) {
                    sym->placement.inc_angle_deg(90);
                }
                else {
                    sym->placement.inc_angle_deg(-90);
                }
            }
            else {
                sym->placement.mirror = !sym->placement.mirror;
            }
            sym->symbol.apply_placement(sym->placement);

        } break;

        case ObjectType::TRACK: {
            if (!rotate) {
                auto &track = core.b->get_board()->tracks.at(it.uuid);

                if (track.layer == BoardLayers::TOP_COPPER)
                    track.layer = BoardLayers::BOTTOM_COPPER;
                else if (track.layer == BoardLayers::BOTTOM_COPPER)
                    track.layer = BoardLayers::TOP_COPPER;
            }
        } break;

        case ObjectType::BOARD_PACKAGE: {
            auto brd = core.b->get_board();
            BoardPackage *pkg = &brd->packages.at(it.uuid);
            transform(pkg->placement.shift, center, rotate);
            if (rotate) {
                pkg->placement.inc_angle_deg(-90);
            }
            else {
                pkg->flip ^= true;
                pkg->placement.invert_angle();
                brd->expand_flags = Board::EXPAND_PACKAGES;
                brd->packages_expand.insert(it.uuid);
            }

        } break;

        case ObjectType::HOLE: {
            Hole *hole = core.r->get_hole(it.uuid);
            transform(hole->placement.shift, center, rotate);
            if (rotate) {
                hole->placement.inc_angle_deg(-90);
            }
        } break;

        case ObjectType::PAD: {
            Pad *pad = &core.k->get_package()->pads.at(it.uuid);
            transform(pad->placement.shift, center, rotate);
            if (rotate) {
                pad->placement.inc_angle_deg(-90);
            }
        } break;

        case ObjectType::BOARD_HOLE: {
            BoardHole *hole = &core.b->get_board()->holes.at(it.uuid);
            transform(hole->placement.shift, center, rotate);
            if (rotate) {
                hole->placement.inc_angle_deg(-90);
            }
        } break;

        case ObjectType::SHAPE: {
            Shape *shape = &core.a->get_padstack()->shapes.at(it.uuid);
            transform(shape->placement.shift, center, rotate);
            if (rotate) {
                shape->placement.inc_angle_deg(-90);
            }
        } break;

        case ObjectType::NET_LABEL: {
            auto sheet = core.c->get_sheet();
            auto *label = &sheet->net_labels.at(it.uuid);
            label->orientation = transform_orientation(label->orientation, rotate);
        } break;
        case ObjectType::BUS_LABEL: {
            auto sheet = core.c->get_sheet();
            auto *label = &sheet->bus_labels.at(it.uuid);
            label->orientation = transform_orientation(label->orientation, rotate);
        } break;
        default:;
        }
    }
    if (core.b && core.b->get_board()->packages_expand.size()) {
        core.b->get_board()->expand();
    }
}
} // namespace horizon
