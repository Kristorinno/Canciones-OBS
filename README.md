# Canciones OBS

Plugin para OBS Studio que permite proyectar letras de canciones en el lienzo, con control desde el celular y editor de temas avanzado.

## Características
- **Control de estrofas:** Flechas, teclado numérico y F9 (ocultar letra).
- **Control Remoto:** Servidor web local con código QR para controlar desde el celular.
- **Editor de temas:** Personalización total de fuentes, colores, bordes, sombras y efectos.
- **Lista de reproducción:** Organizá tus canciones para cada sesión.
- **Buscador inteligente:** Encuentra canciones por título, artista o letra (ignorando tildes).

## Requisitos de compilación
- Windows 10/11 (64-bit)
- CMake 3.28 o superior
- Visual Studio 2022 (con soporte para C++)
- Qt 6 (Core, Widgets, Network)
- OBS Studio 31+ (con los headers de desarrollo y `libobs`)

## Cómo compilar
1. Clonar el repositorio.
2. Abrir una terminal en la carpeta raíz y ejecutar `cmake --preset windows-x64` (si usás Visual Studio 2022).
3. Compilar con `cmake --build --preset windows-x64`.
4. Copiar el archivo `canciones-obs.dll` generado a la carpeta de plugins de OBS (`C:\ProgramData\obs-studio\plugins\canciones-obs\bin\64bit\`).

## Atajos de teclado
- **F4:** Abrir/Minimizar la ventana de Canciones OBS (configurable en Ajustes > Atajos de OBS).
- **Flechas:** Estrofa anterior / siguiente.
- **F9:** Ocultar/Mostrar letra.
- **0 al 9:** Proyectar la estrofa correspondiente.
- 
