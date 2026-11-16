# FAROL

Roguelike de póker original para PSP (pensado para la pantalla de 480x272 de la PSP 3010).
Juegas manos de póker para sumar `fichas x Multi`, superas 8 mesas de 3 rondas y entre rondas
compras en la tienda.

## Qué hay en el juego

- **52 amuletos** pasivos (máximo 5 a la vez) de tres rarezas, cada uno con su ilustración.
- **Dos capas de modificadores por carta**: la *marca* actúa al puntuar (Plata, Rubí, Cristal, Oro)
  y la *muesca* en otros momentos (Eco, Ancla, Trueque).
- **18 trucos**: objetos de un solo uso que se gastan durante la ronda. Se pueden vender en la
  tienda, y dos de ellos (Sobre lacrado y Chuleta) también usarse allí.
- **17 rivales** con regla propia en la tercera ronda de cada mesa; los más duros solo salen en mesas altas.
- **5 barajas iniciales** (cuatro desbloqueables) y **3 dificultades** (ganar abre la siguiente).
- **Modo infinito**: al superar la octava mesa se puede seguir jugando con objetivos cada vez más altos.
- **Guardado automático** en los datos guardados de la consola, colección y récords.

## Controles

| Botón | En la partida | En la tienda |
|---|---|---|
| Cruceta / stick | Mover el cursor (arriba/abajo: elegir/soltar) | Moverse por la lista |
| X | Elegir o soltar carta (mantener: acelerar el recuento) | Comprar; vender un amuleto tuyo; usar un truco tuyo |
| Cuadrado | Jugar las cartas elegidas | Vender un truco tuyo |
| Triángulo | Descartar las cartas elegidas | |
| Círculo | Soltar todas | |
| L | Cambiar el orden de la mano (número / palo) | |
| R | Abrir los trucos: primero elige las cartas, luego R y X para usarlo | |
| SELECT | Abrir o cerrar la información; L/R cambian de página (manos, amuletos y trucos, mazo) | Igual |
| START | Pausa (música sí/no, salir al título) | |

En la colección, L y R cambian de página y Círculo vuelve. Al elegir baraja, izquierda y derecha
cambian la dificultad.

## Guardado

La partida se guarda sola al empezar cada ronda y al entrar en la tienda, usando los datos
guardados del sistema: aparece en *Juego > Datos guardados* como "FAROL" (carpeta
`ms0:/PSP/SAVEDATA/FARL000010000`). Si apagas la consola a mitad de una ronda, al continuar vuelves
al principio de esa ronda. El mismo guardado contiene los récords, la colección y lo desbloqueado;
para empezar de cero basta con borrarlo desde ese menú.

Si la utilidad de guardado del sistema fallara, el juego usa como respaldo un archivo suelto en
`ms0:/PSP/COMMON/FAROL.SAV`.

## Compilar

Hace falta el toolchain [pspdev](https://pspdev.github.io/) y Python 3.

Con Docker (Windows):

```powershell
.\build.ps1
```

A mano, con pspdev instalado:

```bash
make
python tools/mkiso.py farol.prx FAROL.iso
```

El resultado es `FAROL.iso` (y también `EBOOT.PBP` por si se prefiere lanzarlo desde `PSP/GAME`).

Las ilustraciones se dibujan con letras en `tools/art.txt`. Después de cambiarlas hay que
regenerar `src/art.h` (el segundo argumento es una hoja de vista previa opcional):

```bash
python tools/mkart.py vista.png
```

## Pruebas

Las reglas y el guardado se compilan para PC y se prueban sin consola (incluye partidas simuladas
con cada baraja):

```bash
gcc -O2 -Wall -Isrc tests/test_game.c src/game.c src/save.c -o test_game && ./test_game
```

El sintetizador de sonido también se prueba en PC, y deja un WAV para escucharlo:

```bash
gcc -O2 -Wall -Isrc tests/test_synth.c src/synth.c -o test_synth && ./test_synth muestra.wav
```

Para el emulador hay un modo de autoprueba en el que un bot juega por la interfaz real y el juego
guarda capturas en `ms0:/farol_*.raw` (480x272, RGBA):

- `make clean && make AUTOTEST=1` — partida completa desde un perfil vacío.
- `make clean && make AUTOTEST=2` — se detiene en la primera tienda y deja una partida guardada.
- `make clean && make AUTOTEST=3` — lee ese guardado, elige "Continuar" y comprueba dónde aparece.
- `make clean && make AUTOTEST=4` — gana todas las rondas: victoria y dos mesas de modo infinito.

Con PPSSPP hay que activar el renderizado por software para que las capturas no salgan en negro.
Recuerda hacer `make clean && make` después para volver a la versión normal.

## Instalar en la PSP

Copia `FAROL.iso` a la carpeta `ISO/` de la Memory Stick. Un ISO casero necesita custom firmware
(por ejemplo 6.61 PRO o ARK-4). También se puede abrir con el emulador PPSSPP.

## Estructura

- `src/game.c` — reglas: mazo, evaluación de manos, puntuación, rivales, tienda, trucos. No depende
  de la PSP. Los amuletos son una tabla de datos: cada uno tiene hasta dos reglas
  *cuándo / condición / efecto*, así que añadir uno nuevo es añadir una fila.
- `src/save.c` — perfil y partida guardada, con suma de comprobación.
- `src/savedata.c` — guardado en la PSP: datos guardados del sistema, con archivo de respaldo.
- `src/ui.c` — pantallas, animaciones y flujo de la partida.
- `src/gfx.c`, `src/input.c`, `src/main.c` — dibujo con sceGu, mando y arranque.
- `src/synth.c`, `src/audio.c` — efectos de sonido y música generados por código; no hay archivos de audio.
- `src/art.h` — ilustraciones, generadas desde `tools/art.txt`.
- `tools/mkiso.py` — genera `PARAM.SFO`, el icono y la imagen ISO.
