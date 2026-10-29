Import("env")
import os
import shutil

def get_build_flag_value(flag_name):
    build_flags = env.ParseFlags(env['BUILD_FLAGS'])
    flags_with_value_list = [build_flag for build_flag in build_flags.get('CPPDEFINES') if type(build_flag) == list]
    defines = {k: v for (k, v) in flags_with_value_list}
    return defines.get(flag_name)

def after_build(source, target, env):
    if os.environ.get("SKIP_FIRMWARE_COPY") == "1":
        print("Skipping custom firmware copy (SKIP_FIRMWARE_COPY=1)")
        return
    print( "Executing custom step " )
    dir    = env.GetLaunchDir()
    name   = env.get( "PIOENV" )
    build_dir = env.subst("$BUILD_DIR")

    # Gateway

    if name == "gateway32pro-tft" :
        target = dir + "/bin/firmware32pro-tft.bin"
        source = os.path.join(build_dir, "firmware.bin")
        print( "Copy file : " + source + " -> " + target )
        shutil.copyfile( source, target )

        target = dir + "/bin/partitions32pro-tft.bin"
        source = os.path.join(build_dir, "partitions.bin")
        print( "Copy file : " + source + " -> " + target )
        shutil.copyfile( source, target )

    elif name == "gateway32s3pro-tft-sd" :
        target = dir + "/bin/firmware32s3pro-tft-sd.bin"
        source = os.path.join(build_dir, "firmware.bin")
        print( "Copy file : " + source + " -> " + target )
        shutil.copyfile( source, target )

        target = dir + "/bin/partitions32s3pro-tft-sd.bin"
        source = os.path.join(build_dir, "partitions.bin")
        print( "Copy file : " + source + " -> " + target )
        shutil.copyfile( source, target )

    elif name == "gateway32s3wave-tft-sd" :
        target = dir + "/bin/firmware32s3wave-tft-sd.bin"
        source = os.path.join(build_dir, "firmware.bin")
        print( "Copy file : " + source + " -> " + target )
        shutil.copyfile( source, target )

        target = dir + "/bin/partitions32s3wave-tft-sd.bin"
        source = os.path.join(build_dir, "partitions.bin")
        print( "Copy file : " + source + " -> " + target )
        shutil.copyfile( source, target )

print( "Adding custom build step (copy firmware): ")
env.AddPostAction("buildprog", after_build)
