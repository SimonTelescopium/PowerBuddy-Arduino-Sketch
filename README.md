# PowerBuddy-Arduino-Sketch
<b>An Arduino sketch to implement an ASCOM switch interface</b><P>

<B>The Power Buddy by SimonTelescopium has three components</B><p>
<OL>
<LI>PowerBuddy Arduino Sketch<p>
This is the code in this repository. This is the sketch for the Arduino based relay controller, containing the sketch and instructions for how to build the PowerBuddy and configure the sketch for your usecase</LI>
<LI>PowerBuddy ASCOM Switch Driver<p>
This creates and installs an ASCOM compliant driver to be used in conjunction with the Arduino PowerBuddy Sketch</LI>
<li>PowerBuddy ASCOM Client<p>
This is client software to control the PowerBuddy, this software is optional, but I strongly suggest you use it to at least test all the components before using PowerBuddy with other ASCOM switch clients </LI>
</OL>
<b>Features and limitations</b>
<ul>
<LI>Supports up to 8 switches/relays/sensors</li>
<LI>Only Supports binary (on/Off) switches/relays/sensors </li>
<LI>Configurable power on state for each relay/switch saved in EEPROM</li>
<LI>Names and descriptions saved in EEPROM</li>
<LI>can mix read only and read write switches</li>
<LI>Doesn't support analog switches</li>
<LI>Relay wireing configurable in sketch</li>
</ul>

All configuration items are fully documented in the Arduino sketch.
