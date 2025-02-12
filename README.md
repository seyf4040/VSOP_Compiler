# VSOP_Compiler
 This repository contains the implementation of a VSOP (Very Simple Object-Oriented Programming) compiler. The compiler implements the diefferent requirements available in Resources/Statements folder

## Usage 
Pull Docker Image (one time)
```
docker pull cffs/compilers
```

Run Docker Container (with current directory shared, available at /home/vagrant/compilers)
```
docker run --rm -it -v "%cd%:/home/vagrant/compilers" cffs/compilers /bin/sh
```

Go to user vagrant then navigate to home directory
```
su vagrant
cd /home/vagrant
```

## Project structure 
```
VSOP_COMPILER
|--resources/                # Resource documentation
|   |--statements/           # Statements of project
|--src/                      # Source code
|   |--Lexical analysis/     # 
|   |--Semantic analysis/    #

```

## Running 
Navigate to the part you need to run 
- Lexical analysis 
```
./vsopc input.vsop
```

## Contributors 
- Mparirwa Julien
- Ural Seyfullah