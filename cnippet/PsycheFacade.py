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


import sys
from CompilerFacade import *
from Diagnostics import *
from Process import *


class PsycheFacade:
    """ Facade to the Psyche inference engine """

    _id = 'psychec'

    def __init__(self, cnip_opt):
        self.generator = 'psychecgen'
        self.solver = 'psychecsolver-exe'
        self.no_typedef = cnip_opt['no_typedef']
        self.no_heuristic = cnip_opt['no_heuristic']
        self.no_stdlib = cnip_opt['no_stdlib']
        self.host_cc = cnip_opt['host_cc']


    def generate(self, unit, cc_opts):
        """ Invoke the constraint generator. """
        cmd = [self.generator,
               unit.c_file_path,
               '-o',
               unit.cstr_file_path]

        cmd += ['--cc', self.host_cc]
        cmd += CompilerFacade.predefined_macros('--cc-D')
        cmd += CompilerFacade.undefined_macros('--cc-U')
        cmd += ['--cc-std', cc_opts.c_version]

        maybe_append('--no-typedef', self.no_typedef, cmd)
        maybe_append('--no-heuristic', self.no_heuristic, cmd)

        if not self.no_stdlib:
            cmd.append('-p')
            cmd.append('libpsychecstd')

        ok = call_process(PsycheFacade._id, cmd)
        if ok != 0:
            sys.exit(DiagnosticReporter.fatal(CONSTRAINT_GENERATION_FOR_FILE_FAILED, unit.c_file_path))


    def solve(self, unit):
        """ Invoke the constraint solver. """
        cmd = [self.solver,
               '--',
               '-i',
               unit.cstr_file_path,
               '-o',
               unit.cnip_file_path]

        if not self.no_stdlib:
            cmd.append('--match-stdlib=approx')

        ok = call_process(PsycheFacade._id, cmd)
        if ok != 0:
            sys.exit(DiagnosticReporter.fatal(CONSTRAINT_SOLVING_FOR_FILE_FAILED, unit.c_file_path))