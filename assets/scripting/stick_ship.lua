local thruster = 0
local radar = 0

return {
    name = "Stick Ship",
    construct = function()
        place(BLOCK_HUB, 0, 0)
        radar = place(BLOCK_RADAR, 1, 0)
        place(BLOCK_GUN, 2, 0)
        place(BLOCK_GUN, 3, 0)

        thruster = place(BLOCK_THRUSTER, -1, 0)
    end,

    update = function()
        thruster_set(thruster, 100)

        local radar_angle = radar_angle(radar)
        radar_rotate(radar, -20.0)
    end,
}
