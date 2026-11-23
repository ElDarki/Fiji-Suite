# Fiji Suite

Lienzos extra, transmisión a varias plataformas y decks de botones para [OBS Studio](https://obsproject.com).

[Read in English](README.md)

Fiji Suite permite tener varios lienzos en un mismo OBS, por ejemplo una versión horizontal y otra vertical
del mismo directo. Cada lienzo tiene sus propias escenas, su propio modo estudio y sus propias salidas de
transmisión, grabación, búfer de repetición y cámara virtual. También añade decks: tableros de botones que
ejecutan pasos en OBS, y que se pueden pulsar desde OBS, el navegador o el celular.

> **Beta.** Fiji Suite está en la versión 0.x y todavía puede tener errores. Exporta tu colección de escenas
> antes de probarlo (**Colección de escenas → Exportar**) y avísame de cualquier fallo.

## Qué hace

**Lienzos**
- Lienzos extra, cada uno en su propio panel, con sus escenas, fuentes y transiciones.
- Modo estudio por lienzo: el botón **Studio Mode** de un lienzo lo divide en Vista previa y Programa. Al
  elegir una escena solo cambia la vista previa, hasta que se pulsa **Transition**.
- Atajos de teclado por lienzo para la transición y para activar o desactivar el modo estudio, y una opción
  para intercambiar vista previa y programa después de una transición.

**Salidas y multistream**
- Transmisión, grabación, búfer de repetición y cámara virtual para cada lienzo, desde el panel **Outputs**.
  Puedes transmitir a Twitch, YouTube, TikTok, Kick y otras al mismo tiempo.
- Grupos de salidas: clic derecho en una salida para meterla en un grupo, y luego iniciar o detener todo el
  grupo de una vez, desde el panel o con un atajo de teclado.

**Multiview**
- Un panel con todos los lienzos en vivo, uno al lado del otro, con las salidas de cada uno, su tiempo en
  directo, bitrate y fotogramas perdidos.
- Con clic derecho se abre a pantalla completa en otro monitor, o se muestra en lugar de la vista previa
  principal.

**Decks**
- Tableros de botones. Cada botón ejecuta pasos con tiempos: cambiar de escena, mostrar, ocultar o mover
  fuentes, activar filtros, cambiar textos, silenciar audio, reproducir sonidos, con variables, esperas y
  condiciones.
- Los botones se pulsan desde un panel de OBS, cualquier navegador, el celular (en la misma red), un atajo de
  teclado o un bot.
- Un editor de decks y un importador de decks de SAMMI, que avisa de lo que no pudo convertir.
- Detalles (en inglés): [docs/decks.md](docs/decks.md).

**Otras herramientas**
- Copiar y pegar entre fuentes de cualquier lienzo, desde el clic derecho de la fuente: propiedades (entre
  fuentes del mismo tipo), transformación y filtros.
- Control remoto por el servidor WebSocket de OBS, para bots y stream decks:
  [docs/websocket.md](docs/websocket.md) (en inglés).
- Conexión opcional con el bot de Twitch Fiji-chan. Está apagada mientras no se ponga la dirección del bot, y
  todo lo demás funciona sin él.

**Buscar actualizaciones**
- Cuando OBS termina de abrir, Fiji Suite pregunta una sola vez a GitHub si hay una versión nueva y, si la
  hay, ofrece abrir la página de descarga. Es la única petición que hace por su cuenta y no envía nada sobre ti
  ni tu configuración. Para apagarlo, desmarca **Check for Fiji Suite updates when OBS starts** en Fiji Suite
  Settings.

**Temas Fiji Sun y Fiji Moon**
- Dos temas para OBS que siguen la guía de estilo de Fiji-chan: **Fiji Sun** (una página de manga en una playa
  soleada: papel cálido, líneas de tinta, verde agua) y **Fiji Moon** (un lounge de noche: índigo, líneas doradas,
  títulos en Mincho), con sus fuentes, botones de píldora, pestañas tipo chip y sus casillas. Funcionan en OBS 31,
  32 y 33.
- Fiji Suite los copia a la carpeta de temas de OBS. OBS solo busca temas nuevos al abrir, así que reinícialo
  una vez después de instalar (Fiji Suite te lo recuerda) y elige uno en **Ajustes → Apariencia → Tema**.
  Nada cambia hasta que elijas uno.

Fiji Suite añade sus propios paneles y menús. No reorganiza la ventana de OBS.

## Requisitos

- OBS Studio 31.1 o más nuevo.
- Windows 10 u 11 (64 bits), o Linux (OBS de Flatpak, o el OBS del PPA oficial en Ubuntu 24.04).
- macOS todavía no.

## Instalación

Descarga el archivo para tu sistema desde la
[última versión](https://github.com/ElDarki/Fiji-Suite/releases/latest). Cierra OBS antes de instalar.

### Windows

1. Descomprime `fiji-suite-VERSION-windows-x64.zip` en la carpeta `C:\ProgramData\obs-studio\plugins\`.
2. Comprueba que existe este archivo: `C:\ProgramData\obs-studio\plugins\fiji-suite\bin\64bit\fiji-suite.dll`.
3. Abre OBS.

Windows o el antivirus pueden avisar de que el archivo no está firmado. Las versiones beta no están firmadas.

### Linux con OBS de Flatpak

Para el OBS de Flathub (`com.obsproject.Studio`), en cualquier distribución (CachyOS, Arch, Fedora, Mint,
etc.):

```
flatpak install --user fiji-suite-VERSION-linux-flatpak.flatpak
```

Si tu OBS está instalado para todo el sistema y ese comando da error, usa el mismo comando con `sudo` y sin
`--user`.

### Linux con OBS sin Flatpak

Compilado en Ubuntu 24.04 con el OBS del PPA oficial. En otras distribuciones puede no cargar; en ese caso
usa el OBS de Flatpak.

```
mkdir -p ~/.config/obs-studio/plugins
tar -xzf fiji-suite-VERSION-linux-x86_64.tar.gz -C ~/.config/obs-studio/plugins
```

### Actualizar

Cierra OBS e instala la nueva versión de la misma forma, encima de la anterior. Tus lienzos, salidas y decks
se conservan.

### Desinstalar

- Windows: borra la carpeta `C:\ProgramData\obs-studio\plugins\fiji-suite`.
- Flatpak: `flatpak uninstall com.obsproject.Studio.Plugin.FijiSuite`.
- Linux sin Flatpak: borra `~/.config/obs-studio/plugins/fiji-suite`.

## Primeros pasos

Todo está en el menú **Herramientas** (Tools) de OBS:

- **Add Canvas** crea un lienzo nuevo con su panel.
- **Fiji Suite Settings** tiene los lienzos, salidas, atajos y la conexión con el bot.
- **Multiview** abre el panel multiview; **Multiview Over Main Preview** lo muestra en lugar de la vista
  previa principal.
- **Decks** abre las páginas de los decks.

Los paneles se muestran u ocultan desde el menú **Paneles** (Docks), como cualquier panel de OBS. La interfaz
del plugin está en inglés por ahora.

## Importante

- **No uses Fiji Suite al mismo tiempo que Aitum Stream Suite.** Los dos manejan los mismos lienzos y
  salidas y chocan. Quita o desactiva uno de los dos.
- Mientras Fiji Suite esté en beta, haz una copia de tu colección de escenas antes de probar una versión
  nueva.

## Si algo falla

Abre un [issue en GitHub](https://github.com/ElDarki/Fiji-Suite/issues). Incluye:

- El registro de OBS: **Ayuda → Archivos de registro → Subir el archivo de registro actual**, y pega el
  enlace.
- Si OBS se cerró solo: el informe de **Ayuda → Informes de fallos**.
- Qué estabas haciendo cuando pasó, y tus versiones de Fiji Suite y de OBS.

## Compilar

Las instrucciones para compilar están en el [README en inglés](README.md#build-from-source).

## Autor

Hecho por darkzeromex.

- Twitch: [twitch.tv/darkzeromex](https://www.twitch.tv/darkzeromex)
- X: [x.com/darkzeromex](https://x.com/darkzeromex)
- Código: [github.com/ElDarki/Fiji-Suite](https://github.com/ElDarki/Fiji-Suite)

## Licencia y origen

GNU General Public License v2, ver [LICENSE](LICENSE).

Fiji Suite es una versión modificada de Aitum Stream Suite, de Aitum y Exeldro, a partir del commit
`8d39111` (noviembre de 2026). La GPL exige que este aviso de origen y modificación acompañe al código.
