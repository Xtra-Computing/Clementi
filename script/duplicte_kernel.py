#!/usr/bin/python3
import argparse

import re
import sys
import copy

parser = argparse.ArgumentParser()
parser.add_argument('--np', type=int, default=1,
                help="number of pipeline")
parser.add_argument('--input', type=str, default='template.cfg',
                help="input file")
parser.add_argument('--output', type=str, default='kernel.cfg',
                help="output file")
args = parser.parse_args()


num_of_cu =args.np

fin = open(args.input, "rt")
fout = open(args.output, "wt")

regex = r"^stream_connect=(\w+).(\w+):(\w+).(\w+)"

for line in fin:

    test_comment =line.replace('#', '!#')
    if(test_comment != line):
        continue

    test_nk =line.replace('nk=', '!nk=')
    if(test_nk != line):
        for i in range(num_of_cu):
            out_line=line[0:-2] + str(i + 1) + '\n'
            fout.write(out_line)
    else :
        test_sc = line.replace('stream_connect=', '!stream_connect=')
        if (test_sc != line):
            matches = re.finditer(regex, line, re.MULTILINE)
            src = dst = []
            for matchNum, match in enumerate(matches, start=1):
                src = match.group(1)
                src_port_name = match.group(2)
                dst = match.group(3)
                dst_port_name = match.group(4)

            for i in range(num_of_cu):
                new_src = src[0:-1] + str(i + 1)
                new_dst = dst[0:-1] + str(i + 1)

                fout.write(line.replace(src, new_src).replace(dst, new_dst))
        else :
            test_slr = line.replace('slr=', '!slr=')
            if (test_slr != line):
                for i in range(num_of_cu):
                    replace_slr =  line.replace('SLR0', 'SLR' + str(i))
                    new_slr = replace_slr.replace('_1:', '_' + str(i + 1) + ':')
                    fout.write(new_slr);

            else :
                test_sp = line.replace('sp=', '!sp=')
                if (test_sp != line):
                    for i in range(num_of_cu):
                        replace_ddr =  line.replace('DDR[0]', 'DDR[' + str(i) + ']')
                        new_ddr = replace_ddr.replace('_1.', '_' + str(i + 1) + '.')
                        fout.write(new_ddr);
                else :
                    fout.write(line);



fin.close()

fout.close()