Import("env")
import os
import shlex

# Fix Windows SCons response file argument parsing in GCC
orig_spawn = env['SPAWN']

def custom_spawn(sh, escape, cmd, args, spawn_env):
    new_args = []
    for arg in args:
        if arg.startswith("@") and os.path.exists(arg[1:]):
            temp_file = arg[1:]
            try:
                with open(temp_file, "r", encoding="utf-8", errors="ignore") as f:
                    raw_content = f.read()
                
                # Use shlex to parse arguments with spaces properly
                tokens = shlex.split(raw_content, posix=False)
                formatted = "\n".join(tokens)
                
                with open(temp_file, "w", encoding="utf-8") as f:
                    f.write(formatted)
            except Exception as e:
                pass
        new_args.append(arg)
    return orig_spawn(sh, escape, cmd, new_args, spawn_env)

env['SPAWN'] = custom_spawn
