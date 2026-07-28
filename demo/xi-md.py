import sys
import yaml
if __name__=="__main__":
    f = open(sys.argv[1])
    x = yaml.full_load(f)
    print(x)
    f.close()
