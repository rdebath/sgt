% Sokoban mode for Jed
% 
% Layers on top of any other mode, and redefines the number and
% app-keypad keys as Sokoban push commands, treating the current
% cursor position as the player. (The diagonals are supported, but
% not to do anything vanilla Sokoban would not allow.)
% 
%  - Wall characters are #%+-|
%  - Space character is space
%  - Pit character is .
%  - ^ is considered a destructive pit (push a rock into it and
%    it vanishes, as in NetHack's Sokoban)
%  - Everything else is considered a rock. Uppercase rocks become
%    lowercase when pushed into pits; lowercase rocks leave pits
%    behind them when pushed. Caseless rocks may not be pushed into
%    non-destructive pits.

variable SOKOBAN_SPACE = 0, SOKOBAN_PIT = 1;
variable SOKOBAN_ROCK = 2, SOKOBAN_WALL = 3;
variable SOKOBAN_DESTRUCTIVE_PIT = 4;

variable sokoban_space_char = ' ';
variable sokoban_pit_char = '.';

define sokoban_class(c) {
    if (c == '#' or c == '%' or c == '+' or c == '-' or c == '|')
	return SOKOBAN_WALL;
    if (c == sokoban_space_char)
	return SOKOBAN_SPACE;
    if (c == sokoban_pit_char)
	return SOKOBAN_PIT;
    if (c == '^')
	return SOKOBAN_DESTRUCTIVE_PIT;
    return SOKOBAN_ROCK;
}

define sokoban_passable(class) {
    if (class == SOKOBAN_SPACE or class == SOKOBAN_PIT)
	return 1;
    return 0;
}

define sokoban_caseless(char) {
    if (char >= 'a' and char <= 'z')
	return 0;
    if (char >= 'A' and char <= 'Z')
	return 0;
    return 1;
}

define sokoban_toupper(char) {
    if (char >= 'a' and char <= 'z')
	return char - 32;
    return char;
}

define sokoban_tolower(char) {
    if (char >= 'A' and char <= 'Z')
	return char + 32;
    return char;
}

define sokoban_space_from_rock(char) {
    if (char >= 'a' and char <= 'z')
	return sokoban_pit_char;
    return sokoban_space_char;
}

define sokoban_debug(msg) {
    message(msg);
}

define sokoban_diag_move(dx, dy) {
    variable col, line;
    !if (sokoban_passable(sokoban_class(what_char()))) {
	sokoban_debug("Cannot make a Sokoban move while not on a space.");
	return;
    }
    line = what_line();
    col = what_column();
    goto_line(line+dy); goto_column(col+dx);
    !if (sokoban_passable(sokoban_class(what_char()))) {
	goto_line(line); goto_column(col);
	sokoban_debug("Can't perform a Sokoban diagonal move into a non-space.");
	return;
    }
    % Now look for at least one way through
    goto_line(line+dy); goto_column(col);
    if (sokoban_passable(sokoban_class(what_char()))) {
	goto_line(line+dy); goto_column(col+dx);
	return;
    }
    goto_line(line); goto_column(col+dx);
    if (sokoban_passable(sokoban_class(what_char()))) {
	goto_line(line+dy); goto_column(col+dx);
	return;
    }
    goto_line(line); goto_column(col);
    sokoban_debug("Can't perform a Sokoban diagonal move through a diagonal gap.");
}

define sokoban_move(dx, dy) {
    variable c0, c1, cc1, c2, cc2;
    variable col, line;
    c0 = what_char();
    !if (sokoban_passable(sokoban_class(c0))) {
	sokoban_debug("Cannot make a Sokoban move while not on a space.");
	return;
    }
    line = what_line();
    col = what_column();
    goto_line(line+dy); goto_column(col+dx);
    c1 = what_char();
    cc1 = sokoban_class(c1);
    if (sokoban_passable(cc1)) {
	% Moved from space to space; no difficulty.
	return;
    }
    if (cc1 == SOKOBAN_WALL) {
	goto_line(line); goto_column(col);
	sokoban_debug("Cannot make a Sokoban move into a wall.");
	return;
    }
    if (cc1 == SOKOBAN_ROCK) {
	goto_line(line+2*dy); goto_column(col+2*dx);
	c2 = what_char();
	cc2 = sokoban_class(c2);
	if (cc2 == SOKOBAN_DESTRUCTIVE_PIT) {
	    c1 = sokoban_space_from_rock(c1);
	    c2 = sokoban_space_char;
	} else {
	    !if (sokoban_passable(cc2)) {
		goto_line(line); goto_column(col);
		sokoban_debug("Cannot make a Sokoban move that pushes a" +
			      " rock into a non-space.");
		return;
	    }
	    if (cc2 == SOKOBAN_PIT) {
		if (sokoban_caseless(c1)) {
		    goto_line(line); goto_column(col);
		    sokoban_debug("Cannot Sokoban-push a caseless rock" +
				  " into a pit.");
		    return;
		}
		c2 = sokoban_tolower(c1);
		c1 = sokoban_space_from_rock(c1);
	    } else {
		if (sokoban_caseless(c1)) {
		    c2 = c1;
		} else {
		    c2 = sokoban_toupper(c1);
		}
		c1 = sokoban_space_from_rock(c1);
	    }
	}
	deln(1); insert_char(c2);
	goto_line(line+dy); goto_column(col+dx);
	deln(1); insert_char(c1);
	goto_line(line+dy); goto_column(col+dx);
	% This leaves the cursor on the new space, which is correct.
    }
}

define sokoban_move_up() { sokoban_move(0, -1); }
define sokoban_move_down() { sokoban_move(0, 1); }
define sokoban_move_left() { sokoban_move(-1, 0); }
define sokoban_move_right() { sokoban_move(1, 0); }

define sokoban_move_upleft() { sokoban_diag_move(-1, -1); }
define sokoban_move_upright() { sokoban_diag_move(1, -1); }
define sokoban_move_downleft() { sokoban_diag_move(-1, 1); }
define sokoban_move_downright() { sokoban_diag_move(1, 1); }

define sokoban_keydefs(keymap) {
    % Number keys
    definekey("sokoban_move_upleft", "7", keymap);
    definekey("sokoban_move_up", "8", keymap);
    definekey("sokoban_move_upright", "9", keymap);
    definekey("sokoban_move_left", "4", keymap);
    definekey("sokoban_move_right", "6", keymap);
    definekey("sokoban_move_downleft", "1", keymap);
    definekey("sokoban_move_down", "2", keymap);
    definekey("sokoban_move_downright", "3", keymap);

    % App keypad keys
    definekey("sokoban_move_upleft", "\eOw", keymap);
    definekey("sokoban_move_up", "\eOx", keymap);
    definekey("sokoban_move_upright", "\eOy", keymap);
    definekey("sokoban_move_left", "\eOt", keymap);
    definekey("sokoban_move_right", "\eOv", keymap);
    definekey("sokoban_move_downleft", "\eOq", keymap);
    definekey("sokoban_move_down", "\eOr", keymap);
    definekey("sokoban_move_downright", "\eOs", keymap);
}

define sokoban() {
    variable oldkeymap, newkeymap;
    oldkeymap = what_keymap();
    if (strlen(oldkeymap) > 8) {
	if (substr(oldkeymap, strlen(oldkeymap)-7, 8) == "-sokoban") {
	    sokoban_debug("Already in Sokoban mode.");
	    return;
	}
    }
    newkeymap = oldkeymap + "-sokoban";
    !if (keymap_p(newkeymap)) {
	copy_keymap(newkeymap, oldkeymap);
	sokoban_keydefs(newkeymap);
    }
    use_keymap(newkeymap);
    sokoban_debug("Sokoban mode active.");
}

define sokoban_quit() {
    variable keymap, oldkeymap;
    keymap = what_keymap();
    if (strlen(keymap) <= 8) {
	sokoban_debug("Not in Sokoban mode.");
	return;
    }
    if (substr(keymap, strlen(keymap)-7, 8) != "-sokoban") {
	sokoban_debug("Not in Sokoban mode.");
	return;
    }
    oldkeymap = substr(keymap, 1, strlen(keymap)-8);
    use_keymap(oldkeymap);
    sokoban_debug("Sokoban mode disabled.");
}

define sokoban_off() { sokoban_quit(); }

% Level for testing:
% 
%  #########
%  #.. *  ^#
%  #..     #
%  #   B   #
%  #  A C  #
%  #   D   #
%  ###  ####
%    ####
