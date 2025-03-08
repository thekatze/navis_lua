local left_thruster = 0
local right_thruster = 0

return {
    name = "BIG CAT",
    construct = function()
        place(BLOCK_HUB, 0, 0)
        place(BLOCK_HULL, -1, 0)
        place(BLOCK_HULL, -1, 1)
        place(BLOCK_HULL, -1, -1)
        left_thruster = place(BLOCK_THRUSTER, -2, -1)
        right_thruster = place(BLOCK_THRUSTER, -2, 1)
    end,
    update = function()
        set_thruster(left_thruster, 100)
        set_thruster(right_thruster, 100)
    end,
}
