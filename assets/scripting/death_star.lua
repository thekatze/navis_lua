local size = 9
local half_size = math.floor((size - 1) / 2)

local guns = {}

return {
    name = "Death Star",
    construct = function()
        place(BLOCK_HUB, 0, 0)
        for y = -half_size, half_size do
            for x = -half_size, half_size do
                if not (x == 0 and y == 0) then
                    local gun_id = place(BLOCK_GUN, x, y)
                    table.insert(guns, gun_id)
                end
            end
        end
    end,

    update = function()
        for _, gun in ipairs(guns) do
            gun_rotate(gun, 5.0)
            if (gun_cooled_down(gun)) then
                gun_shoot(gun)
            end
        end
    end,
}
