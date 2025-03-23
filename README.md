# Hot-edge for Hyprland
### What
Hot-edge is trigger that is activated when the mouse pointer touches an edge of the monitor. 
### Why
Useful to configure auto-hide bars and panels that only shows when mouse touches the edge it is anchored on.
### How
You can configure as many edges as you want as follows:
```
hotedge = <monitor>,<side>,<activateZoneSize>,<deactivateZoneSize>,<activateCommand>,<deactivateCommand>,<dodgeWindow>
```
- monitor
  - Which monitor is the edge on (e.g. DP-1)
- side
  - Which side the edge is on (top, bottom, left, right)
- activateZoneSize
  - The width of the trigger zone along the edge that activates the activateCommand when the mouse enters it
- deactivateZoneSize
  - Should be **bigger** than the activateZoneSize, triggers deactivateCommand when the mouse leaves the zone
- activateCommand
  - Command to execute when the activate zone triggers
  - Should be whatever command that shows the panel
- deactivateCommand
  - Same as above but for deactivate zone
  - Should be whatever command that hides the panel
- dodgeWindow (0 for false, 1 for true)
  - An optional feature:
    - If no window is overlapping with the active zone, the edge is always active
    - If there is windows overlapping with the active zone, the edge behaves as usual

### Example
Configuring a auto-hide dock:
```
hotedge = DP-2,bottom,8,128,ags request "show dock",ags request "hide dock",1
```
