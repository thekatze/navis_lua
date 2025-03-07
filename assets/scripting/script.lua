local i = 10;
return {
    name = "BIG CAT",
    update = function()
        i = i + 2
        print("updating in lua", i);
    end,
}
