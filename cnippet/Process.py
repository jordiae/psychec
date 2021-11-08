# -----------------------------------------------------------------------------
# Copyright (c) 2017 Leandro T. C. Melo (LTCMELO@GMAIL.COM)
#
# All rights reserved. Unauthorized copying of this file, through any
# medium, is strictly prohibited.
#
# This software is provided on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, explicit or implicit. In no event shall the
# author be liable for any claim or damages.
# -----------------------------------------------------------------------------


import subprocess
import sys
from Algorithms import flatten
from Diagnostics import DiagnosticReporter, EXCEPTION_EXECUTING_PROCESS
from Logger import xtrace
from contextlib import nullcontext
TIMEOUT_PSYCHEC = 20
def execute(parent, cmd, *args, **kwargs):
    """
    Execute an external process with the given command.
    """
    code = -1
    kwargs['timeout'] = TIMEOUT_PSYCHEC
    print(' '.join(cmd))
    # with xtrace(parent, flatten(cmd)) as h:
    with nullcontext:
        try:
            code = subprocess.call(cmd, *args, **kwargs, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
        except BaseException as e:
            print(e)
            pass #sys.exit(
            #    DiagnosticReporter.fatal(EXCEPTION_EXECUTING_PROCESS, cmd[0]))
        #finally:
        #    h.report(code)
    return code

