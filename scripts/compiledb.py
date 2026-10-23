# Regenerate compile_commands.json after every firmware build so clangd stays in sync.
Import("env")
import os

if "compiledb" not in COMMAND_LINE_TARGETS and not os.environ.get("PIO_COMPILEDB_NESTED"):
    def refresh(source, target, env):
        os.environ["PIO_COMPILEDB_NESTED"] = "1"
        env.Execute('"$PYTHONEXE" -m platformio run -e $PIOENV -t compiledb')

    env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", refresh)
