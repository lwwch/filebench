def FlagsForFile(filename, **kwargs):
    return {"flags": ["-x", "c", "-std=c11", "-Wall", "-Wextra", "-Werror"]}
