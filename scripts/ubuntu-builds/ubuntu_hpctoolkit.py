import yaml
externals = {}
with open('/root/.spack/packages.yaml','r') as file:
         externals = yaml.safe_load(file)

del externals['packages']['bzip2']
del externals['packages']['xz']

with open('/root/.spack/packages.yaml','w') as file:
        yaml.dump(externals,file)

with open('/opt/env/spack.yaml','r') as file:
         compilers = yaml.safe_load(file)

compilers['spack']['compilers'][0]['compiler']['paths']['fc'] = '/usr/bin/gfortran'

with open('/opt/env/spack.yaml','w') as file:
         yaml.dump(compilers, file)
