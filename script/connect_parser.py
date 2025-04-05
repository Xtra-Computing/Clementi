#!/usr/bin/env python3
import re
import sys
import copy

from graphviz import Digraph

import operator
import numpy as np

fontsize_all="30pt"
fontsize_cross_slr="60pt"


graph = Digraph('kernel')


graph.attr(rankdir='LR')
graph.attr(ordering="out")
graph.attr(splines="polyline")
#graph.attr(ratio="fill")
#graph.attr(margin="0")
#graph.attr(size="11.7,4.3!")
graph.attr(compound="true")
graph.attr(overlap='false')

graph.attr(layout='dot')
graph.attr(newrank ='true')

# c = copy.deepcopy(a)

n = len(sys.argv)
if n < 2:
    print("need a file")
    exit(-1)

print("Starting ", sys.argv[0])

fin = open(sys.argv[1], "rt")

regex = r"^stream_connect=(\w+).(\w+):(\w+).(\w+)"


regex_for_depth = r"^stream_connect=(\w+).(\w+):(\w+).(\w+):(\w+)"
regex_for_slr = r"^slr=(\w+):SLR(\w+)"

node = {}
edge = {}
slr  = {}

def add_node(node_name, port, child, isin=False):
    global node
    if node_name in node.keys():
        node[node_name][1].append(port)
    else:
        node[node_name] = [node_name,[port],[],[],[0,]]
    if isin:
        node[node_name][2].append(child)
    else:
        node[node_name][3].append(child)


for line in fin:
    if line[0] == '#':
        continue
    matches = re.finditer(regex_for_depth, line, re.MULTILINE)

    for matchNum, match in enumerate(matches, start=1):
        src = match.group(1)
        src_port_name = match.group(2)
        dst = match.group(3)
        dst_port_name = match.group(4)
        key = src + src_port_name + dst + dst_port_name

        edge[key] = [src, dst, src_port_name, dst_port_name, match.group(5)]

    matches = re.finditer(regex, line, re.MULTILINE)

    for matchNum, match in enumerate(matches, start=1):
        src = match.group(1)
        src_port_name = match.group(2)
        dst = match.group(3)
        dst_port_name = match.group(4)
        add_node(src, src_port_name, dst, False)
        add_node(dst, dst_port_name, src, True)
        key = src + src_port_name + dst + dst_port_name

        if not( key in edge.keys()):
            edge[key] = [src, dst, src_port_name, dst_port_name, 1]

    matches = re.finditer(regex_for_slr, line, re.MULTILINE)
    for matchNum, match in enumerate(matches, start=1):
        kernel = match.group(1)
        slr[kernel] = match.group(2);

fork_node_counter =0;
for i in node.values():
    if len(i[3])> 1:
        fork_node_counter +=1;
print("kernel number: " + str(fork_node_counter))




#with graph.subgraph(name='cluster_0') as subgraph:
#    for i in node.values():
#
#        label="<base>" + "Module: " + i[0] + "\l| | "
#
#        for j in i[1]:
#            label = label + "<"+ j +"> Port: " + j + " \l| "
#        label = label+ " "
#        subgraph.node(str(hash(i[0])), label, shape="record")
#        subgraph.attr(label=sys.argv[1])
#        print(i)

slr_value =[(value) for key, value in slr.items()]
list_of_unique_dicts= sorted(list(np.unique(np.array(slr_value))))
print(list_of_unique_dicts)

if (list_of_unique_dicts == []):
    for i in node.values():

        label="<base>" + "Module: " + i[0] + "\l| | "

        for j in i[1]:
            label = label + "<"+ j +"> Port: " + j + " \l| "
        label = label+ " "
        graph.node(str(hash(i[0])), label, shape="record",fontsize=fontsize_all)
        graph.attr(label=sys.argv[1])
        print(i)

    for i in edge.values():

        graph.edge(str(hash(i[0])), str(hash(i[1])),fontsize=fontsize_all, tailport= i[2], headport=i[3], label= ( "d: " + str(i[4])  ),overlap='false')
        print(i)
else:

    for si_string in list_of_unique_dicts:
        si = int(si_string)
        with graph.subgraph(name=('cluster_' + str(si))) as subgraph:
            subgraph.attr(label='SLR' + str(si))
            subgraph.attr(color='black')
            subgraph.node_attr['style'] = 'filled'
            for i in node.values():
                if  int(slr[i[0]]) ==  si:
                    label="<base>" + "Module: " + i[0] + "\l| | "
                    for j in i[1]:
                        label = label + "<"+ j +"> Port: " + j + " \l| "
                    label = label+ " "
                    subgraph.node(str(hash(i[0])), label, shape="record",fontsize=fontsize_all)
            for i in edge.values():
                if (slr[i[0]] ==  slr[i[1]]) and (int(slr[i[0]])  == si):
                    subgraph.edge(str(hash(i[0])), str(hash(i[1])),
                        fontsize=fontsize_all, penwidth="5.0",
                        tailport= i[2], headport=i[3], label= ( "d: " + str(i[4])  ),overlap='false')

        print(i)

    for i in edge.values():
        if (slr[i[0]] !=  slr[i[1]]):
            graph.edge(str(hash(i[0])), str(hash(i[1])),fontsize=fontsize_cross_slr, penwidth="10.0",
                lhead='cluster_'+str(int(slr[i[0]])),
                ltail='cluster_'+str(int(slr[i[1]])),
                tailport= i[2], headport=i[3],
                label= ( "d: " + str(i[4])  ),overlap='true')
        #print(i)


graph.render(sys.argv[1] + ".json", view=False)


fin.close()
