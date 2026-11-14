#!/usr/bin/env python3
"""Shared assembly, packaging and execution for emitter regression tests."""
import os
import pty
import re
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parent.parent
exe = Path(os.environ.get('XLT88', root/'xlt88'))
assembler = os.environ.get('ASM86', 'cpm86_asm86')
gencmd = os.environ.get('GENCMD', 'cpm86_gencmd')
emu = os.environ.get('EMU2', 'emu2')
env = dict(os.environ, EMU2_CPM_VER='2.2', EMU2_DOSVER='1.0')

def runner(work, target):
    def command(args, execution=False, codes=(0,), input=None):
        result = subprocess.run(args, cwd=work, env=env if execution else dict(env, EMU2_DOSVER="3.3"), capture_output=True, timeout=60, input=input)
        assert result.returncode in codes, (args, result.stdout, result.stderr)
        return result.stdout
    def build(name, text, flavor, dialect="intel"):
        # Never let a failed tool leave a previous executable looking successful.
        for extension in ('.obj','.exe','.com','.cmd','.h86'):
            (work/(name+extension)).unlink(missing_ok=True)
        source = work/(name+'.80')
        source.write_bytes(text) if isinstance(text, bytes) else source.write_text(text)
        suffix = '.a86' if flavor in ('asm86','drasm86') else '.asm'
        command([str(exe),'--dialect='+dialect,'--target='+target,'--emit='+flavor,str(source),name+suffix])
        if flavor == 'asm86':
            log = command([assembler,name+'.a86'])
            # Ignored syntax is still a regression, even if the wrapper accepts it.
            assert b'ERROR NO:' not in log and b'NUMBER OF ERRORS:   0' in log, log
            command([gencmd,name,'8080'])
            if target == 'msdos':
                # Combined CMD: one code group, 128-byte header, 100h prefix.
                image = (work/(name+'.cmd')).read_bytes()
                assert image[0] == 1 and image[9] == 0 and image[3:5] == b'\0\0'
                size = int.from_bytes(image[1:3], 'little') * 16
                assert 256 < size <= len(image)-128
                assert image[128:384] == bytes(256)
                (work/(name+'.com')).write_bytes(image[384:128+size])
        elif flavor in ('masm3','drasm86'):
            if flavor == 'masm3':
                log = command([os.environ.get('MASM3','pcdev_masm3'),name+'.asm,'+name+'.obj;'])
                assert re.search(rb'Errors\s+Errors\s+0\s+0',log) and b'E r r o r' not in log, log
            else:
                log = command([os.environ.get('RASM86','pcdev_rasm86'),name])
                assert re.search(rb'Number of errors:\s+0\.',log) and b'ERROR NO:' not in log, log
            log = command([os.environ.get('LINK2','pcdev_link2'),name+'.obj,'+name+'.exe;'], codes=(0,1))
            assert b'error' not in log.lower(), log
            warnings = [line.strip() for line in log.splitlines() if b'warning' in line.lower()]
            assert warnings == [b'Warning: no STACK segment'], log
            command([os.environ.get('EXE2BIN','pcdev_exe2bin'),name+'.exe',name+'.com'])
            assert (work/(name+'.com')).stat().st_size > 0
            if target == 'cpm86':
                command([os.environ.get('BIN2CMD','bin2cmd'),name+'.com',name+'.cmd'])
        else:
            command([os.environ.get('NASM','nasm'),'-Werror','-f','bin',name+'.asm','-o',name+'.com'])
            if target == 'cpm86':
                command([os.environ.get('BIN2CMD','bin2cmd'),name+'.com',name+'.cmd'])
    def execute(name, *args, input=None, console=False):
        if console:
            script = work/'keys.txt'
            script.write_bytes(input or b'')
            # Native status needs a terminal; emu2 supplies the keystrokes.
            master, slave = pty.openpty()
            try:
                result = subprocess.run([emu,'-c','8088','-s',str(script),'-i','100','-d','1',name+('.cmd' if target == 'cpm86' else '.com'),*args], cwd=work, env=env, stdin=slave, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=60)
                assert result.returncode == 0, (result.stdout,result.stderr)
                return result.stdout
            finally:
                os.close(slave)
                os.close(master)
        return command([emu,name+('.cmd' if target == 'cpm86' else '.com'),*args], execution=True, input=input)
    return command, build, execute
