pkg load statistics
clear all


for scale = 17:21

for ef = 1:6
    edge_factor = 2^ef;
    rand ("seed", 103);

    ijw = kronecker_generator (scale, edge_factor);

    G = sparse(ijw(1,:) + 1, ijw(2,:) + 1, ones (1, size (ijw, 2)));

    [ei,ej] = find(G>0);

    dim = size(ei);
    edge_num = dim(1);

    filename = sprintf("/data/graph_dataset/rmat/rmat-%d-%d.txt",scale,edge_factor);
    fp=fopen(filename,'w');

    for i = 1 : edge_num
        fprintf(fp,"%d %d \n",ei(i) , ej(i));
    end
    fprintf("done for %d-%d \n",scale,edge_factor)
end

end

fclose(fp)
