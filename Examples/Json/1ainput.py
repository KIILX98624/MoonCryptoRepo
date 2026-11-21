import os
import json
import time

n1 = int(input("n1: "))
n2 = int(input("n2: "))

print(f"{n1:g}, {n2:g}")

dados = {"n1": n1,"n2": n2}

#caminho = os.path.join(os.path.dirname(os.path.abspath(__file__)), "2conversor.json")

with open("2conversor.json", "w", encoding="utf-8") as arquivo:
    json.dump(dados, arquivo, indent=4)


time.sleep(1)

with open("2conversor.json", "r", encoding="utf-8") as arquivo1:
    dados = json.load(arquivo1)

print(json.dumps(dados, indent=4, ensure_ascii=False))

time.sleep(3)

os.system("cls")