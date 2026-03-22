It looks good, but this needs to improve:
1. Clean the code, for each widget there should be the following functions, each one with the arguments: x,y,size, position (relative to the group it's inside or absolute), allignment (this is where the x,y starts, it can be top-left, top-right, center-left, center-right, bottom-left, bottom-right or center). 
These are the widgets, with some additional arguments between brackets, some of them grouped in containers, also with the x,y,size arguments:

    - Datetime
    - Update time
    - Current weather
        - Weather icon
        - Current Temperature
            - Value
            - Unit        
        - Min/Max Temperature
    - Metrics
        - Metric (sunrise/sunset, pressure, wind, UV index, Humidity and Air Quality). 
            - Icon
            - Title
            - Value
            - Unit / Description
    - 12h trend. Special widget containing the trendline of temperatures and probabilities of rain for the next 12h with 1h granularity.
    - Daily forecast
        - Day forecast
            - Week's day
            - Icon
            - Min/Max

2. The update time should have the icon refresh.png horizontally alligned with the text immediatelly right to it.
3. The wind direction is still not converted to an icon rotation: if for example the wind direction is 45º or NE, the icon should be rotated 45º clockwise.
