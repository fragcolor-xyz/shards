# load the dll from this path
import ctypes, os
temp = ctypes.cdll.LoadLibrary(os.path.dirname(__file__) + "/chainblocks")

# load python stuff
from .cbtypes import *
from .cbchain import *
from .cblocks import *
