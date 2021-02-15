from distutils import dir_util
Import("env")
dir_util.copy_tree("patch",".")
