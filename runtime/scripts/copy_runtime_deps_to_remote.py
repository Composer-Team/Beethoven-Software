import os
import subprocess

print("Remote address (user@host):  ")
remote_address = input()

my_dir = os.environ['BEETHOVEN_ROOT'] + "/Beethoven-Hardware/vsim/generated-src/"
remote_beethoven_root = subprocess.run(["ssh", remote_address, "echo $BEETHOVEN_ROOT"], stdout=subprocess.PIPE).stdout.decode('utf-8').strip()
subprocess.run(["rsync", "-avz",
                "--include=*.cc", "--include=*.hpp", "--include=*.c", "--include=*.h", "--exclude=*",
                ".",
                remote_address + ":" + remote_beethoven_root + "/Beethoven-Hardware/vsim/generated-src/"], cwd=my_dir)