# Regenerate compile_commands.json after every firmware build so clangd stays in sync.
# Skipped on CI: the nested `pio run` there removes the platform as "incompatible"
# on a cold cache and deletes firmware.elf before firmware.bin is built.
Import("env")
import os

if (
    "compiledb" not in COMMAND_LINE_TARGETS
    and not os.environ.get("PIO_COMPILEDB_NESTED")
    and not os.environ.get("CI")
):
    def refresh(source, target, env):
        os.environ["PIO_COMPILEDB_NESTED"] = "1"
        env.Execute('"$PYTHONEXE" -m platformio run -e $PIOENV -t compiledb')

    env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", refresh)
