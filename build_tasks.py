import subprocess


# compile C files
if sys.platform.startswith('win'):
    shell = True
else:
    shell = False


# build client side application