for file in *.{comp,vert,frag}
do
	echo $file
	glslangValidator -V $file -o ${file%.*}.spv
done
