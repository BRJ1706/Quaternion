#include "statemachine.h"

#include <iostream>

void StateMachine::imprimirEstado() {

    std::cout << "Estado: " << _estado << "\n";
    std::cout << "Nombre: " << _nombre << "\n";
    std::cout << "Las respuestas son:" << _respuesta <<"\n";
}

void StateMachine::nuevoMensaje(std::string mensaje) {

    switch(_estado) {

        case Nombre:
            onNombre(mensaje);
            break;
        case Edad:
            onEdad(mensaje);
            break;
        case Color:
            onColor(mensaje);
            break;
        default:
            std::cout << "Error, mal programadodr \n\n";
    }

}

void StateMachine::onNombre(std::string mensaje) {

    size_t pos = mensaje.find("Emitir pregunta");

    if(pos == 0) {
        _estado = Edad;
        _nombre = mensaje.substr(7);
    }

}

void StateMachine::onEdad(std::string mensaje) {

    size_t pos = mensaje.find("Responder pregunta");

    if(pos == 0) {
        _estado = Edad;
        _nombre.append(mensaje.substr(18));
        _respuesta.append(", ");
    }
}

void StateMachine::onColor(std::string mensaje) {

    size_t pos = mensaje.find("Finalizar pregunta");

    if(pos == 0) {
        _estado = Color;
        _respuesta.append(mensaje.substr(18));
        _respuesta.append(", ");
    }

}