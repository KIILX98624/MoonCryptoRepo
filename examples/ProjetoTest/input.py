import os
import json
import time

os.system("cls")

n1 = int(input("n1: "))
n2 = int(input("n2: "))

print(f"{n1:g}, {n2:g}")

dados = {
    "n1": n1,
    "n2": n2
}

caminho = os.path.join(os.path.dirname(os.path.abspath(__file__)), "conversor.json")

with open(caminho, "w", encoding="utf-8") as arquivo:
    json.dump(dados, arquivo, indent=4)


time.sleep(1)

print("Local do arquivo:", os.path.abspath(caminho))
print("Arquivo salvo com sucesso!")

