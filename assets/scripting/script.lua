return {
    name = "BIG CAT",
    construct = function()
        place(BLOCK_HUB, 0, 0)
        place(BLOCK_HULL, 1, 1)
        place(BLOCK_HULL, 1, 2)
        place(BLOCK_HULL, 1, 3)
        place(BLOCK_HULL, -1, 0)
    end,
    update = function()
    end,
}
