Import('env')
import shutil
import os

def post_program_action(source, target, env):
    #print(env["PROJECT_DIR"])
    #print(env["BUILD_DIR"])
    #print(env["PROGNAME"])
    #src = target[0].get_abspath()
    #dest = os.path.join(env["PROJECT_DIR"], "$BUILD_DIR/${PROGNAME}.bin")

    os.makedirs(os.path.dirname(os.path.join(env["PROJECT_DIR"], "fw")), exist_ok=True)
    fw_path = os.path.join(env["PROJECT_DIR"], "fw", env["PROGNAME"]+".bin")
    src = os.path.join(env["PROJECT_DIR"], ".pio", "build", env["PROGNAME"], env["PROGNAME"]+".bin")
    shutil.copy(src, fw_path)


#env.Replace(PROGNAME="firmware_%s" % env.GetProjectOption("custom_prog_version"))
env.Replace(PROGNAME="%s" % env.subst("$PIOENV"))
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", post_program_action)