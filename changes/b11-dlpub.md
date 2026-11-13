## changelog
### 1.0.0-b10
- **Changed (CPU, from DUELL 1.2 by OSCILL/R):** the cost figures that place the second core's deadline are now worked out by the main loop once a frame and handed to the audio through a double buffer. Before, the audio interrupt worked them out itself at the start of every half-buffer (113 to 127 us of each half on the unit, about 4 %), in the block that most often runs late. A sound, engine, multi or project load hands that work back to the audio until the main loop's next pass, so a fresh load never runs on old figures.
