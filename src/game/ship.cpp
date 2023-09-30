#include "ship.h"

bool DynamicSolidTree::BoxCollisionTest(irect2 box, std::function<bool(const Game::Entity &e)> entity_filter) const
{
    return aabb_tree.CollideAabb(box, [&](Tree::NodeIndex node_index)
    {
        auto &e = game.get(aabb_tree.GetNodeUserData(node_index));
        if (entity_filter && !entity_filter(e))
            return false;

        return e.get<DynamicSolid>().BoxCollisionTest(box);
    });
}

bool DynamicSolidTree::ShipBlocksCollisionTest(const ShipPartBlocks &ship, ivec2 ship_offset, std::function<bool(const Game::Entity &e)> entity_filter) const
{
    return aabb_tree.CollideAabb(ship.CalculateRect() + ship_offset, [&](Tree::NodeIndex node_index)
    {
        auto &e = game.get(aabb_tree.GetNodeUserData(node_index));
        if (entity_filter && !entity_filter(e))
            return false;

        return e.get<DynamicSolid>().ShipBlocksCollisionTest(ship, ship_offset);
    });
}

void ShipPartBlocks::Tick()
{
    // std::cout << (map.CollidesWithMap(game.get<MapObject>()->map, pos - game.get<MapObject>()->pos)) << '\n';
}

void ShipPartBlocks::PreRender() const
{
    // Bounding box:
    // r.iquad(pos - game.get<Camera>()->pos, map.cells.size() * ShipGrid::tile_size).color(fvec3(0)).alpha(0.1f);

    map.Render<TileDrawMethods::RenderMode::pre>(game.get<Camera>()->pos - pos);

    // ID:
    // r.itext(pos - game.get<Camera>()->pos + map.cells.size() * ShipGrid::tile_size / 2, Graphics::Text(Fonts::main, FMT("{}", dynamic_cast<const Game::Entity &>(*this).id().get_value()))).align(ivec2(0)).color(fvec3(1,0,0));
}

void ShipPartBlocks::Render() const
{
    // Bounding box:
    // r.iquad(pos - game.get<Camera>()->pos, map.cells.size() * ShipGrid::tile_size).color(fvec3(0)).alpha(0.1f);

    map.Render<TileDrawMethods::RenderMode::normal>(game.get<Camera>()->pos - pos);

    // ID:
    // r.itext(pos - game.get<Camera>()->pos + map.cells.size() * ShipGrid::tile_size / 2, Graphics::Text(Fonts::main, FMT("{}", dynamic_cast<const Game::Entity &>(*this).id().get_value()))).align(ivec2(0)).color(fvec3(1,0,0));
}

int ShipPartPiston::DistanceToPoint(ivec2 point) const
{
    ivec2 a = game.get_link<"a">(*this).get<ShipPartBlocks>().pos + pos_relative_to_a + ivec2::axis(!is_vertical, ShipGrid::tile_size / 2);
    int relative_pos = point[is_vertical] - a[is_vertical];
    int length = game.get_link<"b">(*this).get<ShipPartBlocks>().pos[is_vertical] + pos_relative_to_b[is_vertical] - a[is_vertical];

    int ret = clamp_min(abs(point[!is_vertical] - a[!is_vertical]) - ShipGrid::tile_size / 2);
    if (relative_pos < 0)
        clamp_var_min(ret, -relative_pos);

    if (relative_pos > length)
        clamp_var_min(ret, relative_pos - length);

    return ret;
}

ShipPartPiston::ExtendRetractStatus ShipPartPiston::ExtendOrRetract(bool extend, bool gravity_tweaks)
{
    constexpr int min_length = ShipGrid::tile_size;

    int current_length = (game.get_link<"b">(*this).get<ShipPartBlocks>().pos[is_vertical] + pos_relative_to_b[is_vertical])
        - (game.get_link<"a">(*this).get<ShipPartBlocks>().pos[is_vertical] + pos_relative_to_a[is_vertical]);

    if (!extend && current_length <= min_length)
        return ExtendRetractStatus::at_min_length; // At minimal length.

    ConnectedShipParts parts_a = FindConnectedShipParts(this, true);
    ConnectedShipParts parts_b = FindConnectedShipParts(this, false);

    // Those don't touch `entity_ids`, which is exactly what we want here.
    parts_a.pistons.erase(this);
    parts_b.pistons.erase(this);

    auto map = game.get<MapObject>().get_opt();
    auto tree = game.get<DynamicSolidTree>().get_opt();

    ivec2 offset_a = ivec2::axis(is_vertical, extend ? -1 : 1);
    ivec2 offset_b = ivec2::axis(is_vertical, extend ? 1 : -1);

    bool can_move_a = !CollideShipParts(parts_a, offset_a, map, tree, parts_a.LambdaNoSuchEntityHere());
    bool can_move_b = !CollideShipParts(parts_b, offset_b, map, tree, parts_b.LambdaNoSuchEntityHere());

    bool ground_a = false;
    bool ground_b = false;

    if (gravity_tweaks)
    {
        ivec2 gravity(0,1);

        // Need the same lambda for both.
        // Imagine if A rubs the ground, but B rubs only A. If the lambdas were different,
        // they'd both have ground flags, which is wrong. Only A should have it in this case.
        auto lambda = [a = parts_a.LambdaNoSuchEntityHere(), b = parts_b.LambdaNoSuchEntityHere()](const Game::Entity &e)
        {
            return a(e) && b(e);
        };
        ground_a = offset_a == gravity ? !can_move_a : CollideShipParts(parts_a, gravity, map, tree, lambda);
        ground_b = offset_b == gravity ? !can_move_b : CollideShipParts(parts_b, gravity, map, tree, lambda);
    }

    if (!can_move_a && !can_move_b)
        return ExtendRetractStatus::stuck;

    bool move_b = !can_move_a || (can_move_b && (ground_a > ground_b || (ground_a == ground_b && dir_flip_flop)));
    dir_flip_flop = !dir_flip_flop;

    // Force move A when B sits on the floor.
    if (is_vertical && !extend && CollideShipParts(parts_b, -offset_b, map, tree, parts_b.LambdaNoSuchEntityHere()))
        move_b = false;

    if (move_b)
        MoveShipParts(parts_b, offset_b);
    else
        MoveShipParts(parts_a, offset_a);

    // We exclude this piston from `parts_{a,b}`, so we need to manually update it.
    UpdateAabb();

    return ExtendRetractStatus::ok;
}

void ShipPartPiston::Tick()
{

}

void ShipPartPiston::RenderLow(bool pre) const
{
    constexpr int extra_halfwidth = ShipGrid::tile_size / 2, width = ShipGrid::tile_size * 2, segment_length = ShipGrid::tile_size * 4;

    ivec2 pos_a = game.get_link<"a">(*this).get<ShipPartBlocks>().pos + pos_relative_to_a - game.get<Camera>()->pos - ivec2::axis(!is_vertical, extra_halfwidth);
    ivec2 pos_b = game.get_link<"b">(*this).get<ShipPartBlocks>().pos + pos_relative_to_b - game.get<Camera>()->pos - ivec2::axis(!is_vertical, extra_halfwidth);

    int remaining_pixel_len = pos_b[is_vertical] - pos_a[is_vertical];
    int num_segments = (remaining_pixel_len + segment_length - 1) / segment_length;

    for (int i = 0; i < num_segments; i++)
    {
        ivec2 sprite_size(clamp_max(segment_length, remaining_pixel_len), width);
        remaining_pixel_len -= segment_length;

        auto quad = r.iquad(pos_a + ivec2::axis(is_vertical, segment_length * i),
            "ship_tiles"_image with(= (_.a + ivec2(1,2) * ShipGrid::tile_size + ivec2(segment_length * pre, 0)).rect_size(sprite_size))).pixel_center(fvec2{});
        if (is_vertical)
            quad.flip_x(is_vertical).matrix(ivec2(0,-1).to_rotation_matrix());
    }

    // ID:
    // r.itext(last_rect.center() - game.get<Camera>()->pos, Graphics::Text(Fonts::main, FMT("{}", dynamic_cast<const Game::Entity &>(*this).id().get_value()))).align(ivec2(0)).color(fvec3(1,0,0));
}

void ShipPartPiston::PreRender() const
{
    RenderLow(true);
}
void ShipPartPiston::Render() const
{
    RenderLow(false);
}

void DecomposeToComponentsAndDelete(ShipPartBlocks &self)
{
    // Whether this tile is not empty and not special for splitting purposes.
    auto IsRegularTile = [&](ShipGrid::Tile tile)
    {
        auto piston = ShipGrid::GetTileInfo(tile).piston;
        return piston == ShipGrid::PistonRelation::solid_attachable || piston == ShipGrid::PistonRelation::solid_non_attachable;
    };

    Array2D<char/*bool*/, int> visited(self.map.cells.size());

    struct QueuedPiston
    {
        bool is_vertical = false;

        ShipPartBlocks *block_a = nullptr;
        // Absolute pixel position of the starting corner.
        ivec2 abs_pixel_pos_a;

        ShipPartBlocks *block_b = nullptr;
        ivec2 abs_pixel_pos_b;
    };
    // Maps block position to pistons ending here.
    phmap::flat_hash_map<ivec2, std::vector<QueuedPiston>> queued_pistons;

    for (ivec2 tile_pos : vector_range(self.map.cells.size()))
    {
        if (IsRegularTile(self.map.cells.safe_nonthrowing_at(tile_pos).tile) && !visited.safe_nonthrowing_at(tile_pos))
        {
            auto &new_part = game.create<ShipPartBlocks>();

            ivec2 new_part_tile_offset = tile_pos;

            auto lambda = [&](auto &lambda, ivec2 abs_tile_pos) -> void
            {
                if (!self.map.cells.bounds().contains(abs_tile_pos))
                    return;
                const ShipGrid::Tile this_tile = self.map.cells.safe_nonthrowing_at(abs_tile_pos).tile;
                if (!IsRegularTile(this_tile))
                    return;
                if (auto &flag = visited.safe_nonthrowing_at(abs_tile_pos))
                    return;
                else
                    flag = true;

                ivec2 rel_tile_pos = abs_tile_pos - new_part_tile_offset;

                if (!new_part.map.cells.bounds().contains(rel_tile_pos))
                {
                    ivec2 delta = clamp_max(rel_tile_pos, 0);
                    new_part.map.cells.resize(new_part.map.cells.bounds().combine(rel_tile_pos).size(), -delta);
                    new_part_tile_offset += delta;
                    rel_tile_pos -= delta;
                }

                new_part.map.cells.safe_nonthrowing_at(rel_tile_pos) = self.map.cells.safe_nonthrowing_at(abs_tile_pos);

                // Handle pistons arriving here.
                if (auto iter = queued_pistons.find(abs_tile_pos); iter != queued_pistons.end())
                {
                    for (QueuedPiston &elem : iter->second)
                    {
                        ASSERT(!elem.block_b, "Why are we visiting the same piston twice?");

                        if (elem.block_a == &new_part)
                            continue; // This piston links to itself, skip.

                        elem.block_b = &new_part;
                        elem.abs_pixel_pos_b = abs_tile_pos * ShipGrid::tile_size + self.pos;

                        // Make sure A and B are ordered top-left to bottom-right.
                        if (elem.abs_pixel_pos_a[elem.is_vertical] > elem.abs_pixel_pos_b[elem.is_vertical])
                        {
                            std::swap(elem.block_a, elem.block_b);
                            std::swap(elem.abs_pixel_pos_a, elem.abs_pixel_pos_b);

                            elem.abs_pixel_pos_a[elem.is_vertical] += ShipGrid::tile_size;
                            elem.abs_pixel_pos_b[elem.is_vertical] += ShipGrid::tile_size;
                        }
                    }
                }

                // Handle pistons starting here.
                if (ShipGrid::GetTileInfo(this_tile).piston == ShipGrid::PistonRelation::solid_attachable)
                {
                    for (const bool is_vertical : {false, true})
                    for (const bool is_backward : {false, true})
                    {
                        const ShipGrid::Tile piston_tile_type = is_vertical ? ShipGrid::Tile::piston_v : ShipGrid::Tile::piston_h;
                        const ivec2 step = ivec2::axis(is_vertical, is_backward ? -1 : 1);

                        ivec2 piston_tile_pos = abs_tile_pos;

                        while (true)
                        {
                            ivec2 new_pos = piston_tile_pos + step;
                            if (!self.map.cells.bounds().contains(new_pos) || self.map.cells.safe_nonthrowing_at(new_pos).tile != piston_tile_type)
                                break;
                            piston_tile_pos = new_pos;
                        }

                        if (piston_tile_pos != abs_tile_pos)
                        {
                            ivec2 end_tile_pos = piston_tile_pos + step;
                            if (
                                self.map.cells.bounds().contains(end_tile_pos) &&
                                ShipGrid::GetTileInfo(self.map.cells.safe_nonthrowing_at(end_tile_pos).tile).piston == ShipGrid::PistonRelation::solid_attachable &&
                                !visited.safe_nonthrowing_at(end_tile_pos) // This likely means that the piston has the same part on both sides.
                            )
                            {
                                queued_pistons[end_tile_pos].push_back({
                                    .is_vertical = is_vertical,
                                    .block_a = &new_part,
                                    .abs_pixel_pos_a = (abs_tile_pos + step) * ShipGrid::tile_size + self.pos,
                                });
                            }
                        }
                    }
                }

                for (int i = 0; i < 4; i++)
                    lambda(lambda, abs_tile_pos + ivec2::dir4(i));
            };
            lambda(lambda, tile_pos);

            new_part.pos = self.pos + new_part_tile_offset * ShipGrid::tile_size;

            new_part.UpdateAabb();
        }
    }

    // Add the pistons.
    for (const auto &queued_piston : queued_pistons)
    {
        for (const auto &elem : queued_piston.second)
        {
            ASSERT(elem.block_b, "Why do we have an unfinished piston?");

            auto &new_piston = game.create<ShipPartPiston>();
            game.link<"pistons", "a">(*elem.block_a, new_piston);
            game.link<"pistons", "b">(*elem.block_b, new_piston);
            new_piston.is_vertical = elem.is_vertical;
            new_piston.pos_relative_to_a = elem.abs_pixel_pos_a - elem.block_a->pos;
            new_piston.pos_relative_to_b = elem.abs_pixel_pos_b - elem.block_b->pos;
            new_piston.UpdateAabb();
        }
    }

    game.destroy(self);
}

ConnectedShipParts FindConnectedShipParts(std::variant<ShipPartBlocks *, ShipPartPiston *> blocks_or_piston, std::optional<bool> skip_piston_direction)
{
    ConnectedShipParts ret;

    // The starting piston, if `skip_piston_direction` isn't null.
    ShipPartPiston *half_skipped_piston = nullptr;

    // `prev_piston` is null if this is the first block.
    auto HandleBlocks = [&](auto &HandleBlocks, auto &HandlePiston, ShipPartBlocks &blocks, Game::Id blocks_id, ShipPartPiston *prev_piston) -> void
    {
        if (!ret.blocks.insert(&blocks).second)
            return;
        ret.entity_ids.insert(blocks_id);

        for (const auto &elem : game.get_links<"pistons">(blocks))
        {
            auto &piston = game.get(elem.id()).get<ShipPartPiston>();
            if (&piston == prev_piston)
                continue;
            HandlePiston(HandleBlocks, HandlePiston, piston, elem.id(), &blocks);
            if (ret.cant_skip_because_of_cycle)
                return;
        }
    };
    // `prev_blocks` is null if this is the first piston.
    auto HandlePiston = [&](auto &HandleBlocks, auto &HandlePiston, ShipPartPiston &piston, Game::Id piston_id, ShipPartBlocks *prev_blocks) -> void
    {
        // We don't check the result here, it shouldn't be necessary.
        // It also lets us check for piston cycles when `skip_piston_direction` is non-null below.
        ret.pistons.insert(&piston);
        ret.entity_ids.insert(piston_id);

        if (prev_blocks || !skip_piston_direction || skip_piston_direction.value() != false)
        {
            auto &blocks_entity = game.get_link<"a">(piston);
            if (auto &blocks = blocks_entity.get<ShipPartBlocks>(); &blocks != prev_blocks)
            {
                if (prev_blocks && half_skipped_piston == &piston && skip_piston_direction.value() == true)
                {
                    ret.cant_skip_because_of_cycle = true;
                    return;
                }

                HandleBlocks(HandleBlocks, HandlePiston, blocks, blocks_entity.id(), &piston);
                if (ret.cant_skip_because_of_cycle)
                    return;
            }
        }
        if (prev_blocks || !skip_piston_direction || skip_piston_direction.value() != true)
        {
            auto &blocks_entity = game.get_link<"b">(piston);
            if (auto &blocks = blocks_entity.get<ShipPartBlocks>(); &blocks != prev_blocks)
            {
                if (prev_blocks && half_skipped_piston == &piston && skip_piston_direction.value() == false)
                {
                    ret.cant_skip_because_of_cycle = true;
                    return;
                }

                HandleBlocks(HandleBlocks, HandlePiston, blocks, blocks_entity.id(), &piston);
                if (ret.cant_skip_because_of_cycle)
                    return;
            }
        }
    };

    std::visit(Meta::overload{
        [&](ShipPartBlocks *blocks)
        {
            ASSERT(!skip_piston_direction, "Can't specify `skip_piston_direction` when starting from `ShipPartBlocks`.");
            HandleBlocks(HandleBlocks, HandlePiston, *blocks, dynamic_cast<Game::Entity &>(*blocks).id(), nullptr);
        },
        [&](ShipPartPiston *piston)
        {
            if (skip_piston_direction)
                half_skipped_piston = piston;
            HandlePiston(HandleBlocks, HandlePiston, *piston, dynamic_cast<Game::Entity &>(*piston).id(), nullptr);
        },
    }, blocks_or_piston);

    return ret;
}

bool CollideShipParts(const ConnectedShipParts &parts, ivec2 offset, const MapObject *map, const DynamicSolidTree *tree, std::function<bool(const Game::Entity &e)> entity_filter)
{
    for (const auto &blocks : parts.blocks)
    {
        if (map && blocks->map.CollidesWithMap(map->map, blocks->pos + offset - map->pos))
            return true;

        if (tree && tree->ShipBlocksCollisionTest(*blocks, offset, entity_filter))
            return true;
    }

    for (const auto &piston : parts.pistons)
    {
        if (map && map->map.CollidesWithBox(piston->last_rect + offset - map->pos))
            return true;

        if (tree && tree->BoxCollisionTest(piston->last_rect + offset, entity_filter))
            return true;
    }

    return false;
}

void MoveShipParts(const ConnectedShipParts &parts, ivec2 offset)
{
    for (const auto &blocks : parts.blocks)
    {
        blocks->pos += offset;
        blocks->UpdateAabb();
    }

    // Pistons don't store their position (other than in AABB), so it doesn't need to be updated.
    for (const auto &piston : parts.pistons)
        piston->UpdateAabb();
}
