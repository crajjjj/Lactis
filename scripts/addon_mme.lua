-- Builds the Milk Mod Economy add-on (addons\MME): stages the plugin and the script
-- sources in addons\MME\build, runs Pyro on LactisMME.ppj (compile + zip), and names
-- the archive Release\LactisMME-<ADDON_VERSION>.zip. The add-on is released on its own
-- number, attached to the Oninus Lactis release it was made for.
-- Overrides: PYRO_EXE (pyro.exe path), SKYRIM_GAME_PATH (game root).

local ADDON_VERSION = "1.0.1"

function main()
    local pyro = os.getenv("PYRO_EXE")
    if not pyro or not os.isfile(pyro) then
        local home = os.getenv("USERPROFILE") or ""
        local candidates = os.files(path.join(home,
            ".vscode", "extensions", "joelday.papyrus-lang-vscode-*", "pyro", "pyro.exe"))
        pyro = candidates and candidates[1] or nil
    end
    assert(pyro and os.isfile(pyro),
        "pyro.exe not found - set the PYRO_EXE environment variable")

    local game = os.getenv("SKYRIM_GAME_PATH")
        or "C:\\SteamLibrary\\steamapps\\common\\Skyrim Special Edition"

    local addon = path.join(os.projectdir(), "addons", "MME")
    local build = path.join(addon, "build")
    os.tryrm(build)
    os.mkdir(path.join(build, "Scripts", "Source"))
    os.cp(path.join(addon, "LactisMME.esp"), build)
    os.cp(path.join(addon, "Source", "*.psc"), path.join(build, "Scripts", "Source"))

    os.execv(pyro, {
        "-i", path.join(addon, "LactisMME.ppj"),
        "--game-path", game
    })

    local base = path.join(os.projectdir(), "Release", "LactisMME")
    if os.isfile(base .. ".zip") then
        os.mv(base .. ".zip", base .. "-" .. ADDON_VERSION .. ".zip")
        print("addon: archive -> " .. base .. "-" .. ADDON_VERSION .. ".zip")
    end
end
