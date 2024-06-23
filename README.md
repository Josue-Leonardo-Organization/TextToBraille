# TextToBraille

O projeto proposto visa desenvolver um tradutor de texto para Braille, utilizando uma abordagem que integra hardware e software para transformar textos escritos em uma representação tátil compreensível para deficientes visuais.

## Funcionamento

Uma palavra deve ser digitada através do teclado USB, ao pressionar ENTER a palavra é confirmada e faz com que o ESP32 movimente 6 micro-servos de forma que a primeira letra da palavra digitada é exibida fisicamente em braille, permitindo que o usuário interprete pelo toque.

Para avançar ou retroceder uma letra da palavra digitada, é necessário pressionar o botão a direita(avançar) ou a esquerda(retroceder).

Ao digitar uma nova palavra no teclado e pressionar ENTER novamente, a palavra anterior é sobrescrita com a atual.


## Materiais utilizados

- ESP32
- Protoboard
- Jumpers/ Fios
- Teclado Bluetooth
- 2 Botões
- 6 Micro-Servos

## Exemplo

Palavra “ola”

![image](https://github.com/Josue-Leonardo-Organization/TextToBraille/assets/143439207/b9801444-ecad-4e39-92d6-f131899861c3)

![image](https://github.com/Josue-Leonardo-Organization/TextToBraille/assets/143439207/b20835a5-af9e-45bb-a6a4-9e246e5bb850)

![image](https://github.com/Josue-Leonardo-Organization/TextToBraille/assets/143439207/1786bf5e-fb61-42f6-8e0b-aa49ab966ee9)




## Vídeo demonstrativo

https://youtu.be/wbtmRmHsn7U

[![Vídeo](https://img.youtube.com/vi/wbtmRmHsn7U/maxresdefault.jpg)](https://youtu.be/wbtmRmHsn7U)


