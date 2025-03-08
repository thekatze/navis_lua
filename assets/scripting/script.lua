local left_thruster = 0
local right_thruster = 0
local radar = 0
local gun = 0

return {
    name = "BIG CAT",
    construct = function()
        place(BLOCK_HUB, 0, 0)
        place(BLOCK_HULL, -1, 0)
        place(BLOCK_HULL, -1, 1)
        place(BLOCK_HULL, -1, -1)
        left_thruster = place(BLOCK_THRUSTER, -2, -1)
        right_thruster = place(BLOCK_THRUSTER, -2, 1)
        radar = place(BLOCK_RADAR, 1, 0)
        gun = place(BLOCK_GUN, 2, 0)
    end,

    update = function()
        local ship_angle = ship_angle()
        local x, y = ship_position()
        local dx, dy = ship_velocity()

        -- print("Ship angle:", ship_angle)
        -- print("Ship pos:", x, y)
        -- print("Ship vel:", dx, dy)

        local radar_angle = radar_angle(radar)
        -- print("Radar angle:", radar_angle)
        radar_rotate(radar, -50.0)

        distance = radar_ping(radar)

        local gun_angle = gun_angle(gun)
        -- print("Gun angle:", gun_angle)
        gun_rotate(gun, -100.0)
        if (distance > 0) then
            if (gun_cooled_down(gun)) then
                gun_shoot(gun)
            end
        end

        thruster_set(left_thruster, 0)
        thruster_set(right_thruster, 0)
    end,
}
