add_rules("mode.debug", "mode.release")
set_defaultmode("debug")

set_project("mp_arth")
set_version("1.0.0")

set_languages("c++23")

add_cxflags("-Wno-#warnings", "-Wno-c++26-extensions", "-Wno-error=sign-compare", "-Wno-error=ignored-attributes")
set_warnings("all", "error")

add_ldflags("-fuse-ld=mold")
set_policy("build.ccache", true)

add_syslinks("tbb")

add_includedirs("src", "cmd", "$(builddir)/gen")

local cxx = get_config("cxx") or ""
local toolchain = get_config("toolchain") or ""

if cxx:find("clang") or toolchain:find("clang") then
	print("[INFO]: Detected Clang compiler")
	add_cxflags("-Wno-unused-command-line-argument", "-fcolor-diagnostics", "-fansi-escape-codes")
elseif cxx:find("gcc") or cxx:find("g++") or toolchain:find("gcc") then
	print("[INFO]: Detected GCC compiler")
	add_cxflags("-fdiagnostics-color=always")
end

if is_mode("debug") then
    set_optimize("none")
    add_cxflags("-ggdb", "-fsanitize=address", "-fno-omit-frame-pointer")
    add_ldflags("-fsanitize=address")
else
    set_optimize("fastest")
    add_defines("NDEBUG")
end

target("primesGen")
	set_kind("binary")
	set_default(false)
	add_files("cmd/primesGen.cpp")
	add_files("src/tests/utils/bc_wrapper.cpp")


target("gen_primes_header")
	set_kind("phony")
	add_deps("primesGen")

	set_policy('build.fence', true)

	on_build(function(target)
		import("core.project.config")

		local builddir = config.get("builddir")
		local gen_dir = path.join(builddir, "gen")
		local header_path = path.join(gen_dir, "primes.h")
		local gen_tool = target:dep("primesGen"):targetfile()

		if os.isfile(header_path) and os.isfile(gen_tool) and os.mtime(gen_tool) < os.mtime(header_path) then
		    print("[INFO]: primes.h is up to date.")
		    return
		end

		print("[GEN]: Running primesGen to create header...")
		if not os.exists(gen_dir) then os.mkdir(gen_dir) end

		local out, err = os.iorunv(gen_tool, {"-o", header_path, "-n", "20", "-d", "15"})
		if out and #out > 0 then print(out) end
		if err and #err > 0 then print(err) end
		end)

		on_clean(function(target)
			import("core.project.config")
			local gen_dir = path.join(config.get("builddir"), "gen")
			os.rm(gen_dir)
		end)

for _, filepath in ipairs(os.files("cmd/*.cpp")) do
	local name = path.basename(filepath)
	if name ~= "primesGen" then
		target(name)
			set_kind("binary")
			set_default(true)

			add_files(filepath)
			add_files("src/**.cpp")

			add_deps("primesGen")
	end
end
