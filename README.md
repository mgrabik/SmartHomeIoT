# SmartHomeIoT
This is rule-based system works at home - Smart home.

The architecture consists of 5 IoT nodes that are connected to the server. Nodes are registered at the server using registration messages. Nodes also send reports to the server. In the meantime, nodes wait for response with action from the server. All messages are sent via a custom-designed application layer protocol. The first thing after starting the server is loading all rules from the file named 'rules.txt'. Rules are saved in local database as array of structures.

Type of messages:
<ENROLLING_NEW_DEVICE> - Registration of a new device (node IoT)
<PROVIDING_DATA> - Reporting values from sensors
<COMMAND_DO_SOMETHING> - Command from the server
 
Example rule:
IF (lKitchen, oTrashCan, qDistance < 20)
THEN (lKitchen, oTrashCan, aOPEN)

This means that trash can in the kitchen has a distance sensor and rule says when the distance (measured by a sensor) is less then 20 centimeters perform 'aOPEN' action which means open a cover.
