local thruster_one = 0
local thruster_two = 0

local guns = {}

return {
    name = "danger noodle",
    construct = function()
        place(BLOCK_HUB, 0, 0)
        table.insert(guns, place(BLOCK_GUN, 0, 1))
        table.insert(guns, place(BLOCK_GUN, 0, 2))
        table.insert(guns, place(BLOCK_GUN, 0, 3))
        table.insert(guns, place(BLOCK_GUN, 0, 4))
        table.insert(guns, place(BLOCK_GUN, 0, 5))
        table.insert(guns, place(BLOCK_GUN, 0, 6))
        table.insert(guns, place(BLOCK_GUN, 0, 7))
        table.insert(guns, place(BLOCK_GUN, 0, 8))
        thruster_one = place(BLOCK_THRUSTER, -1, 7)
        thruster_two = place(BLOCK_THRUSTER, -1, 8)
    end,

    update = function()
        thruster_set(thruster_one, 1)
        thruster_set(thruster_two, 1)

        for i, gun in ipairs(guns) do
            if (gun_cooled_down(gun)) then
                gun_shoot(gun)
            end
        end
    end,
}
