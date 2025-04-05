#!/usr/bin/env python3
from graphviz import Digraph
gra = Digraph()

gra.node('a:[port:1]', 'Machine Learning Errors')

gra.node('b', 'RMSE')

gra.node('c', 'MAE')

gra.edges(['ab', 'ac'])

gra.edge('b', 'c', label='subclass')

gra.render('1.pdf', view=True)


from graphviz import Digraph
g = Digraph()
g.node("a00", label="<f0> text | {<f1> text | <f2> text}", shape="record")
g.node("a01", label="<f0> text | {<f1> text | <f2> text}", shape="record")
g.edge('a00', 'a01', tailport='f1', headport='f0')


g.render('2.pdf', view=True)