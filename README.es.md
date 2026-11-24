# MonC

**MonC es un lenguaje de programación experimental con un intérprete/runtime desarrollado en C++20.** Su sintaxis utiliza funciones tipadas, llamadas con corchetes, módulos y un conjunto creciente de estructuras de datos integradas. El proyecto se desarrolla con énfasis en una sintaxis clara, interoperabilidad con bibliotecas nativas e ideas de seguridad de memoria.

> **Estado del proyecto:** MonC es actualmente un prototipo interpretado en evolución. El repositorio contiene un runtime en C++20 que analiza y ejecuta un subconjunto de archivos `.monc`. **Todavía no es un compilador de código nativo.** La compilación nativa, la verificación de ownership por parte del compilador y varias funciones planificadas siguen en desarrollo.

[English](README.md) · [Português (Brasil)](README_PT-BR.md) · [Español](README.es.md)

## Contenido

- [Cómo es la sintaxis de MonC](#cómo-es-la-sintaxis-de-monc)
- [Funciones actuales](#funciones-actuales)
- [Inicio rápido](#inicio-rápido)
- [Compilar el proyecto](#compilar-el-proyecto)
- [Ejecutar un programa](#ejecutar-un-programa)
- [Fundamentos del lenguaje](#fundamentos-del-lenguaje)
- [Módulos integrados](#módulos-integrados)
- [Interoperabilidad con C](#interoperabilidad-con-c)
- [Ejemplos y pruebas](#ejemplos-y-pruebas)
- [Estructura del proyecto](#estructura-del-proyecto)
- [Documentación](#documentación)
- [Limitaciones actuales](#limitaciones-actuales)

## Cómo es la sintaxis de MonC

Un programa sencillo:

```monc
// La función main es el punto de entrada del programa.
fn main[] -> Int32 {
    // Declara una variable entera.
    let primero: Int32 = 20;
    let segundo: Int32 = 22;

    // Muestra el resultado mediante la función integrada print.
    print["La respuesta es: " + (primero + segundo)];

    // Devuelve el código de salida del proceso.
    return 0;
}
```

Una regla importante: **las llamadas a funciones de MonC utilizan corchetes**:

```monc
sumar[10, 20];
print["¡Hola, MonC!"];
crypto.sha256["mensaje"];
```

Las funciones C declaradas mediante `playCodeC` utilizan la sintaxis de llamada de C, con paréntesis.

## Funciones actuales

La tabla describe el prototipo actual, no el diseño final del lenguaje.

| Función | Estado actual |
|---|---|
| Funciones tipadas y punto de entrada `main` | Implementados en el intérprete |
| Variables locales con `let` y constantes con `const` | Implementadas |
| Tipos enteros y cadenas, caracteres y booleanos | Implementados en distintos niveles; consulta la especificación |
| `if`, `else`, `while`, `for`, `switch`, `continue` | Implementados según la cobertura de ejemplos y pruebas |
| Funciones del usuario y módulos de código con `#external` | Implementados |
| Módulo JSON integrado | Implementado |
| `Bytes`, hashes criptográficos, HMAC y bytes aleatorios | Implementados en el runtime actual, con backends específicos por plataforma |
| `ByteArray` mutable y sus métodos documentados | Implementados en la rama de desarrollo actual |
| Algunas funciones C de `math.h` y `stdlib.h` | Interoperabilidad inicial implementada |
| Generación de ejecutables nativos | Planificada, todavía no implementada |
| Ownership y borrowing verificados por el compilador | Planificados, todavía no implementados |
| Compatibilidad completa con los built-ins de Python | No es el objetivo del prototipo; solo se implementó un subconjunto pequeño |

Consulta la [especificación del lenguaje](MDs/MOONLANGUAGE.md) y las notas de [interoperabilidad con C](MDs/C_INTEROP.md) para conocer los detalles y las limitaciones.

## Inicio rápido

### Requisitos

- CMake 3.20 o posterior.
- Un compilador compatible con C++20.
- En Windows, una toolchain MinGW-w64/MSYS2 compatible u otra toolchain C++20.
- En Linux, un compilador C++20 y las bibliotecas de desarrollo necesarias para el proyecto.
- Los archivos de desarrollo de OpenSSL son opcionales en sistemas que no sean Windows. Si están disponibles, CMake habilita el backend criptográfico OpenSSL.

Comprueba que las herramientas estén disponibles:

```sh
cmake --version
g++ --version
```

### Compilar el proyecto

Ejecuta estos comandos desde la raíz del repositorio:

```sh
cmake -S . -B build
cmake --build build --config Release
```

El ejecutable normalmente se encuentra en:

- Windows: `build/Release/monc.exe` o `build/monc.exe`, según el generador.
- Linux: `build/monc`.

### Ejecutar un programa

Pasa un archivo fuente al intérprete:

```sh
./build/monc run "Code MonC/Examples/others/if_else.monc"
```

En Windows, por ejemplo:

```powershell
.\build\Release\monc.exe run "Code MonC\Examples\others\if_else.monc"
```

La ruta exacta del ejecutable depende del generador de CMake. También puedes añadir el directorio del ejecutable a `PATH` para llamar a `monc` desde otras carpetas.

### Ejecutar las pruebas automatizadas

```sh
ctest --test-dir build -C Release --output-on-failure
```

En Windows, el repositorio también incluye un script PowerShell de smoke tests que compila el runtime con MinGW y ejecuta los fixtures:

```powershell
.\Code\MonC\SmokeTest\smoke.ps1
```

Ejecuta el script desde la raíz del repositorio en PowerShell, con el compilador necesario disponible.

## Fundamentos del lenguaje

### Funciones y variables

Los parámetros de las funciones se declaran entre corchetes y el tipo de retorno aparece después de `->`:

```monc
// Recibe dos enteros y devuelve su suma.
fn sumar[a: Int32, b: Int32] -> Int32 {
    return a + b;
}

fn main[] -> Int32 {
    // Declara una variable con un tipo explícito.
    let resultado: Int32 = sumar[12, 30];

    // Muestra el valor calculado.
    print[resultado];
    return 0;
}
```

En algunos casos, el tipo se puede inferir:

```monc
let mensaje = "Hola";
let valores = Array[1, 2, 3];
```

### Condiciones y bucles

```monc
fn main[] -> Int32 {
    let contador: Int32 = 0;

    // Repite mientras contador sea menor que tres.
    while [contador < 3] {
        print[contador];
        contador = contador + 1;
    }

    // Selecciona un bloque según la condición.
    if (contador == 3) {
        print["Terminado"];
    } else {
        print["No terminado"];
    }

    return 0;
}
```

### Arrays

El runtime actual incluye el tipo `Array` y operaciones para trabajar con arrays:

```monc
fn main[] -> Int32 {
    // Crea un array con tres números enteros.
    let numeros: Array<Int32> = Array[10, 20, 30];

    // Añade un valor al final del array.
    array.push[numeros, 40];

    // Lee el elemento de la posición cero.
    print[array.get[numeros, 0]];

    // Muestra la cantidad de elementos del array.
    print[array.length[numeros]];

    return 0;
}
```

### ByteArray

`ByteArray` es una secuencia mutable de bytes, donde cada valor está entre 0 y 255. Es diferente del tipo `Bytes`, utilizado para datos binarios y operaciones criptográficas.

```monc
fn main[] -> Int32 {
    // Crea un ByteArray a partir de un Array.
    let datos = bytearray[Array[10, 20, 30]];

    // Cambia el byte situado en el índice cero.
    datos[0] = 15;

    // Añade dos bytes al final.
    datos.append[40, 50];

    // Consulta la cantidad actual de bytes.
    print[datos.length];

    // Muestra todos los valores almacenados.
    print[datos[]];

    return 0;
}
```

La rama actual también implementa métodos como `pop`, `vpop`, `insert`, `clear`, `copy`, `reverse`, `contains`, `index`, `count`, `extend`, `slice` y `fill`. Las reglas exactas de sus argumentos están en [MOONLANGUAGE.md](MDs/MOONLANGUAGE.md).

## Módulos integrados

### JSON

El módulo integrado `json` permite analizar, crear, leer, escribir y serializar valores JSON:

```monc
#external "json"

fn main[] -> Int32 {
    // Convierte un texto JSON en un JsonValue.
    let documento: JsonValue = json.parse["{\"nombre\":\"MonC\",\"version\":2}"];

    // Obtiene los campos y los convierte a los tipos MonC esperados.
    let nombre: String32 = json.get[documento, "nombre"].as_string[];
    let version: Int32 = json.get[documento, "version"].as_int[];

    // Muestra los valores extraídos.
    print["Nombre: " + nombre];
    print["Versión: " + version];

    return 0;
}
```

Consulta los ejemplos de `Code MonC/Examples/json_external` para ver más operaciones.

### Criptografía

El runtime actual ofrece SHA-256, HMAC-SHA-256 y generación de bytes aleatorios seguros. Ejemplo de hash:

```monc
use crypto

fn main[] -> Int32 {
    // Guarda el texto que se va a procesar.
    let mensaje: String32 = "MonC";

    // Calcula el digest SHA-256.
    let digest = crypto.sha256[mensaje];

    // Convierte el digest a hexadecimal y muestra el resultado.
    print["SHA-256: " + digest.hex[]];

    return 0;
}
```

No consideres disponibles las API de cifrado planificadas solo porque aparezcan en ejemplos antiguos de diseño. Comprueba el código y la especificación actuales antes de depender de una operación criptográfica.

## Interoperabilidad con C

MonC puede declarar y llamar a un conjunto limitado de funciones C nativas mediante `#external` y `playCodeC`:

```monc
#external "stdlib.h" {
    playCodeC {
        int atoi(const char* texto);
        int rand(void);
        void srand(unsigned int seed);
    }
}

fn main[] -> Int32 {
    // Convierte texto decimal en un entero mediante la biblioteca C.
    let numero: Int32 = atoi("123");

    // Establece la semilla antes de solicitar un valor pseudoaleatorio.
    srand(123);
    let valor: Int32 = rand();

    print[numero];
    print[valor];
    return 0;
}
```

Esta integración es limitada. No significa que toda la biblioteca estándar de C ni cualquier header arbitrario sean compatibles. Consulta [MDs/C_INTEROP.md](MDs/C_INTEROP.md) para conocer las firmas y restricciones.

## Ejemplos y pruebas

Los programas de ejemplo y demostración se encuentran en:

`Code MonC/Examples/`

| Carpeta | Contenido |
|---|---|
| `array` | Creación y operaciones con arrays |
| `bytearray` | Arrays mutables de bytes y built-ins |
| `crypto` | Hashes y sintaxis de criptografía |
| `externalLib` | Interoperabilidad con bibliotecas C |
| `functions` | Funciones del usuario y funciones importadas |
| `json_external` | Análisis, serialización y datos JSON externos |
| `math` | Funciones matemáticas |
| `modules` | División del código en archivos MonC |
| `others` | Cadenas, caracteres, constantes, condiciones, bucles y otras demostraciones |

Algunos archivos son experimentos o demostraciones de diseño y pueden utilizar sintaxis que el runtime actual todavía no admite. Para comprobar los comportamientos cubiertos por la suite automatizada, utiliza preferentemente los fixtures de `Code MonC/SmokeTest/fixtures`.

## Estructura del proyecto

```text
.
├── src/                         # Intérprete y runtime en C++20
├── Code MonC/
│   ├── Examples/                # Programas de ejemplo en MonC
│   └── SmokeTest/
│       ├── fixtures/            # Programas utilizados en las pruebas
│       └── smoke.ps1            # Ejecutor de smoke tests para Windows
├── MDs/                         # Documentación detallada del lenguaje
├── editors/vscode/              # Soporte de MonC en VS Code
└── CMakeLists.txt               # Configuración de compilación con CMake
```

## Documentación

- [Guía para principiantes](MDs/GUIA_INICIANTE.md)
- [Guía técnica avanzada](MDs/GUIA_AVANCADO.md)
- [Especificación del lenguaje](MDs/MOONLANGUAGE.md)
- [Interoperabilidad con C](MDs/C_INTEROP.md)
- [Hoja de ruta del proyecto](MDs/ROADMAP_MOONCRYPTO.md)
- [Extensión para VS Code](editors/vscode/README.md)

## Limitaciones actuales

MonC todavía es un prototipo. Entre sus limitaciones importantes:

- El ejecutable actual interpreta código MonC; todavía no genera ejecutables nativos a partir de archivos `.monc`.
- La especificación completa y el comportamiento del runtime siguen evolucionando.
- La interoperabilidad con C admite solo un conjunto limitado de bibliotecas y firmas.
- Las operaciones generales con punteros y el ownership verificado por el compilador todavía no están implementados.
- Algunos ejemplos son experimentos antiguos y pueden no funcionar con el parser actual.
- Los built-ins inspirados en Python forman un subconjunto pequeño, no una compatibilidad completa con Python.

Las contribuciones, los informes de errores y los ejemplos concretos son bienvenidos. Al informar de un problema, incluye el archivo fuente, el comando utilizado, la salida esperada y la salida obtenida.
