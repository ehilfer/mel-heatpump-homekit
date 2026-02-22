import subprocess
Import("env")

def get_git_info(args):
    try:
        return subprocess.check_output(args).decode("utf-8").strip()
    except:
        return "unknown"

# wolfSSL 5.7.6 ships its own user_settings.h in wolfssl/src/.
# PrependUnique ensures the project's include/ is searched FIRST so that
# our user_settings.h wins over wolfSSL's bundled Arduino default.
inc = f"-I{env['PROJECT_DIR']}/include"
env.PrependUnique(CCFLAGS=[inc])
env.PrependUnique(CXXFLAGS=[inc])

env.Append(CPPDEFINES=[
    ("GIT_HASH",     f'\\"{get_git_info(["git", "rev-parse", "--short", "HEAD"])}\\"'),
    ("GIT_DESCRIBE", f'\\"{get_git_info(["git", "describe", "--match", "v*", "--dirty=-x", "--always", "--abbrev=4"])}\\"'),
    ("GIT_COMMITS",  f'\\"{get_git_info(["git", "rev-list", "--count", "HEAD"])}\\"'),
])
