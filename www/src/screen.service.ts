import { Injectable } from '@angular/core';

export const SCREENS: Screen[] = [
	{id: 1, name: 'Dashboard'},
	{id: 11, name: 'Temperatuur'},
	{id: 12, name: 'Energie'},
	{id: 13, name: 'Schakelaars'},
];

@Injectable()
export class Screen {
	id: number;
	name: string;

}

@Injectable()
export class ScreenService {

	getScreens(): Screen[] {
		return SCREENS;
	}

}


