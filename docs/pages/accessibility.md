# Accessibility {#accessibility}

[TOC]

The game and its documentation must be usable by people with disabilities.
This page lists, for each category, what is already in place and what is
planned. It takes inspiration from settings found in commercial games, such as
full control remapping, color blind friendly markers and difficulty options.

## Game

### Physical and motor disabilities

| Measure | Status |
| ------- | ------ |
| Every action accepts up to four controls at once (keyboard keys, gamepad buttons, analog stick directions, mouse buttons), so a player can use the device that suits them. | In place |
| Analog sticks trigger movement past a 30 percent threshold, so a light push is enough. | In place |
| The game needs only five controls: four directions and one fire button. | In place |
| Controls are defined in one place, rtk::Input::InputMapping, so changing them never touches gameplay code. | In place |
| In game menu to remap every action. | Planned |
| Auto fire option, so the fire button does not need to be pressed and released repeatedly. | Planned |

### Audio and visual disabilities

| Measure | Status |
| ------- | ------ |
| No information is given by sound only: every shot, spawn and destruction is visible on screen. | In place |
| Each player ship has its own color, and each player starts on its own row. | In place |
| A number or shape marker on each ship, so players are told apart without relying on color. | Planned |
| Color blind friendly palette for the four ships (the current blue, green, pink and red set is hard to read for red green color blindness). | Planned |
| Separate volume settings for music and effects. | Planned |

### Mental and cognitive disabilities

| Measure | Status |
| ------- | ------ |
| Simple rules and controls: move, shoot, avoid enemies and walls. | In place |
| Menus use a single Confirm action. | In place |
| Difficulty setting (enemy speed and spawn rate). | Planned |
| Pause in solo games. | Planned |

## Documentation

These measures are applied to this website.

| Measure | Why |
| ------- | --- |
| Every diagram is followed by a **text description** that gives the same information. | Readers using a screen reader, or who cannot see the diagram, lose nothing. |
| Diagrams are SVG images. | They stay sharp when zoomed. |
| Light and dark themes follow the reader system preference. | Reduces glare and helps readers with light sensitivity. |
| A clear outline marks the element that has keyboard focus. | The site can be navigated with the keyboard only. |
| The search field and the navigation tree work with the keyboard. | Finding a page does not require a mouse. |
| Pages use a real heading hierarchy and each long page starts with a table of contents. | Screen readers can jump between sections, and every reader gets an overview first. |
| Tables have header rows, and wide tables scroll inside the page. | Data stays readable by assistive tools and on small screens. |
| No information is conveyed by color alone. | Readable by color blind readers and in print. |
| Short sentences, plain English, one term per concept. | Easier for non native speakers and readers with cognitive disabilities. |
| Pages declare English as their language. | Screen readers pick the right pronunciation. |
