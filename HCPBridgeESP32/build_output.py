Import('env')
import shutil
import os

def post_program_action(source, target, env):
    src = target[0].get_abspath()
    dest = os.path.join(env["PROJECT_DIR"], "$BUILD_DIR/${PROGNAME}.bin")
    print("dest:"+dest)
    print("source:"+src)
    shutil.copy(src, dest)


#env.Replace(PROGNAME="firmware_%s" % env.GetProjectOption("custom_prog_version"))
env.Replace(PROGNAME="%s" % env.subst("$PIOENV"))

#for e in (env, DefaultEnvironment()):
    #e.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", post_program_action)
