# Justificación: Uso de TCP en Capa de Transporte

## Decisión

Este proyecto utiliza **TCP (SOCK_STREAM) exclusivamente** para todas las comunicaciones entre clientes y servidor.

## Análisis TCP vs UDP

### Requerimientos del sistema

| Funcionalidad | ¿Requiere confiabilidad? | ¿Requiere orden? | Protocolo necesario |
|---------------|-------------------------|------------------|---------------------|
| Autenticación | Sí (credenciales completas) | Sí | **TCP** |
| Comandos de control | Sí (crítico para seguridad) | Sí | **TCP** |
| Respuestas del servidor | Sí (cliente necesita confirmación) | Sí | **TCP** |
| Telemetría (cada 10s) | No estrictamente | No estrictamente | TCP o UDP |

### ¿Por qué no UDP para telemetría?

Aunque la telemetría podría técnicamente usar UDP (es periódica y la pérdida de un paquete no es tan importante), elegimos TCP porque:

1. **Simplicidad arquitectónica**: Un solo socket por cliente en lugar de dos (TCP para control + UDP para datos)
2. **Overhead (carga adicional) despreciable**: Con intervalo de 10 segundos, el overhead de TCP es insignificante
3. **Sincronización**: Evita inconsistencias entre estado de control y datos de telemetría
4. **Sin necesidad de implementar**: Números de secuencia, control de duplicados, o reordenamiento

### Ventajas de TCP en este contexto

- Conexión persistente: apropiada para sesión de monitoreo continuo
- Control de flujo: servidor no sobrecarga clientes lentos
- Detección de desconexión: sabe cuándo cliente se va
- Implementación más simple: menos código, menos bugs

## Conclusión

Dado que tres de las cuatro funcionalidades del sistema (autenticación, comandos y respuestas) requieren la confiabilidad de TCP, y la telemetría se envía solo cada 10 segundos, usar TCP para todo simplifica el diseño sin afectar el rendimiento.