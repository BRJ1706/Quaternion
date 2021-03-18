#include "statemachine.h"

#include <iostream>

void StateMachine::imprimirEstado() {

    std::cout << "Estado: " << _estado << "\n";
    std::cout << "Nombre: " << _nombre << "\n";
}

void StateMachine::onNombre(std::string mensaje) {

    size_t pos = mensaje.find("nombre:");

    if(pos == 0) {
        _estado = Edad;
        _nombre = mensaje.substr(7);
    }

}

void StateMachine::nuevoMensaje(std::string mensaje) {

    switch(_estado) {

        case Nombre:
            onNombre(mensaje);
            break;
        case Edad:
            _estado = Color;
            break;
        case Color:
            _estado = Nombre;
            break;
        default:
            std::cout << "Error, mal programadodr \n\n";
    }
}