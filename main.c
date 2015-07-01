/**
*                            /!\important - à destination du prof/!\
*
*  Pour pouvoir utiliser le programme, il faut renommer les fichiers SDL.daa en SDL.dll et pthreadGC2.daa en pthreadGC2.dll
*  J'ai renommé les extensions des fichiers pour que le Gmail accepte le mail
*
*  Pour pouvoir compiler le programme, il faut installer deux librairies : SDL v1.2 et Pthread. Pour windows, je donne les fichiers.a et .h dans un dossier "Dev-cpp"
*  Il suffit juste de copier coller le dossier a l'endroit ou Dev-cpp/MinGW est installé sur votre ordinateur, puis de cliquer sur "oui" lorsque windows demande 
*  "fusionner les dossiers?"
*
**/
/*
v1.0
compilé et linké avec GCC 
Arguments de compilation : -O2 -Wall -D_GNU_SOURCE=1 -Dmain=SDL_main -DNO_STDIO_REDIRECT -D_REENTRANT
Arguments du linker : -lmingw32 -lSDLmain -lSDL -lpthreadGC2 [-mwindows eventuellement sous windows si on ne désire pas pas avoir la console]
contact : @gusfl2 (twitter)

Remarque : la seule partie du code qui n'est pas multiplateforme est celle de la détection du nombre de coeurs logiques du CPU
Si on efface cette ligne la et qu'on met un nombre de thread fixe, le programme est multiplateforme
*/

#include <SDL/SDL.h>
#include <pthread.h>
#include <windows.h>

#define FPS 30 //le FPS du logiciel
#define PRECISION_MAX 100 //la précision de calcul
#define VITESSE_ZOOM 4 // la vitesse du zoom
#define NB_THREADS_PAR_PROCESSEUR 1 //le nombre de threads par proecesseur


struct params_dessiner_fractale
{
	int largeur_ecran,hauteur_ecran,debut_ecran_y,fin_ecran_y,*continuer;
	long double debut_x,fin_x,debut_y,fin_y;
	SDL_Surface *ecran;
};//la structure qui contiendra les arguments a passer dans chaque thread.

pthread_mutex_t mutex= PTHREAD_MUTEX_INITIALIZER;//le mutex pour sécuriser la ram. oui, on le déclare en variable globale et c'est parce qu'on a pas le choix. Une variable locale
//est inacessible par definition à plusieurs fonctions (sauf par pointeur normalement. mais ici précisément on peut pas utiliser de pointeur pour un mutex...étant donné que
//le mutex est justement la pour résoudre les problemes d'acces mémoire des pointeurs)

int main(int argc, char *argv[]);
void dessiner_fractale(struct params_dessiner_fractale *params_dessiner_fractale);




int main(int argc, char *argv[])
{
	int temps_precedent=SDL_GetTicks(),//pour la gestion du FPS
	largeur_ecran=500,//la hauteur/largeur de l'ecran
	hauteur_ecran=300,
	continuer=1,//continuer est la variable pour fermer le programme correctement
	clic_gauche=0,//vaut 1 si le clic gauche est enfoncé (permet la gestion multi evenementielle)
	besoin_actualisation=1,//si on a besoin de relancer le dessin de la fractale
	i;

	long double zoom=1.0, debut_x=-2.0, debut_y=-1.0, fin_x=1.0, fin_y=1.0, constante_vitesse_zoom=(long double)VITESSE_ZOOM/200.0;
	register SDL_Surface *ecran;//on déclare la variable directement dans les registres du CPU, car on vas avoir besoin de l'utiliser régulièrement de manière tres rapide.
	
	SDL_Event evenement;
	
	SYSTEM_INFO infos_systemes;
	GetSystemInfo(&infos_systemes);//on récupère le nombre de processeurs logiques et on fait *le nombre de thread par processeur
	int nombre_de_threads= infos_systemes.dwNumberOfProcessors * NB_THREADS_PAR_PROCESSEUR;
	
	pthread_t thread[nombre_de_threads];//on définit autant de thread que néssésaire
	struct params_dessiner_fractale params_dessiner_fractale[nombre_de_threads];//et on définis une structure de parametre pour chaque thread

	
	SDL_Init(SDL_INIT_VIDEO);//on lance la SDL
	ecran = SDL_SetVideoMode(largeur_ecran, hauteur_ecran, 32, SDL_HWSURFACE|SDL_DOUBLEBUF|SDL_RESIZABLE);
	SDL_WM_SetCaption("Fractale de Mandelbrot multicoeur - Augustin F.L",NULL);
		
	
	while(continuer)
    {
		SDL_PollEvent(&evenement);//a chaque tour de boucle : on attend un évenement. Si l'utilisateur veut quitter, on quitte, si il veut resizer la fenetre on adapte le resizing, 
		//si il fait un clic gauche on lance le zoom.
		
		if(evenement.type==SDL_QUIT)continuer=0;
		else if(evenement.type == SDL_VIDEORESIZE)
		{
			hauteur_ecran=evenement.resize.h;//si l'utilisateur aggrandi/reduit la fenetre : on met a jour
			largeur_ecran=evenement.resize.w;
			ecran = SDL_SetVideoMode(largeur_ecran, hauteur_ecran, 32, SDL_HWSURFACE|SDL_DOUBLEBUF|SDL_RESIZABLE);
			besoin_actualisation=1;
		}
		else if(!((evenement.motion.x==0 && evenement.motion.y==0) || ((evenement.active.state & SDL_APPACTIVE) && evenement.active.gain==0)))
		{
			if(     evenement.type==  SDL_MOUSEBUTTONDOWN && evenement.button.button == SDL_BUTTON_LEFT ) clic_gauche=1;
			else if(evenement.type==  SDL_MOUSEBUTTONUP   && evenement.button.button == SDL_BUTTON_LEFT ) clic_gauche=0;
		}
		
		if(clic_gauche)	
		{
			zoom*=(1.0+constante_vitesse_zoom);//zoom multipliactif. : n'augemente pas la vitesse avec le temps
				
			debut_x+=((long double)evenement.motion.x/(long double)largeur_ecran)*constante_vitesse_zoom*(3.0/zoom);//les coordonnées sont mises a jours selon le % ou se trouve la souris sur 
			debut_y+=((long double)evenement.motion.y/(long double)hauteur_ecran)*constante_vitesse_zoom*(2.0/zoom);//l'axe des X (pour zoomer sur la souris. meme concept pour l'axe y)
				
			fin_x-=(1-(long double)evenement.motion.x/(long double)largeur_ecran)*constante_vitesse_zoom*(3.0/zoom);
			fin_y-=(1-(long double)evenement.motion.y/(long double)hauteur_ecran)*constante_vitesse_zoom*(2.0/zoom);
			besoin_actualisation=1;		
		}
		
		
		if(besoin_actualisation)//si on a besoin de redessiner la fractale
		{
			for (i=0;i<nombre_de_threads;i++)
			{
				params_dessiner_fractale[i].debut_x=debut_x;//on crée les parametres pour chaque thread.
				params_dessiner_fractale[i].fin_x=fin_x;
				params_dessiner_fractale[i].debut_y=debut_y;//les intervalles x/y de la fractale a dessiner 
				params_dessiner_fractale[i].fin_y=fin_y;
					
				params_dessiner_fractale[i].ecran=ecran;
				params_dessiner_fractale[i].continuer=&continuer;// <- important : on crée un pointeur vers continuer, on ne recopie pas la variable directement.
				//cela permet que si l'utilisateur désire quitter, alors chaque thread se ferme conformément a l'aide de cette unique variable.
					
				params_dessiner_fractale[i].hauteur_ecran=hauteur_ecran;//la largeur/hauteur de l'écran
				params_dessiner_fractale[i].largeur_ecran=largeur_ecran;
					
				params_dessiner_fractale[i].debut_ecran_y=(int)((long double)i*((long double)hauteur_ecran/(long double)nombre_de_threads));//la partie de l'écran (en pixel) que chaque thread   
				params_dessiner_fractale[i].fin_ecran_y=(int)(((long double)i+1.0)*((long double)hauteur_ecran/(long double)nombre_de_threads));//dois dessiner. Chaque thread dessine un rectangle 
				//par exemple, sur 2 coeurs, le 1er thread dessinera un rectangle de 0 a hauteur_ecran/2, et le 2eme de hauteur_ecran/2 a hauteur_ecran.
				
				if(pthread_create(&thread[i],NULL,(void*) dessiner_fractale,&params_dessiner_fractale[i])==EAGAIN) printf("erreur: impossible de creer les threads !! :(\n");
				SetThreadPriority(&thread[i],THREAD_PRIORITY_HIGHEST);//on lance chaque thread et on le met en priorité juste 1 cran en dessous du maximum ("haute").
			}
			
			for (i=0; i < nombre_de_threads; i++)  pthread_join(thread[i], NULL);// on attend la fin de chaque thread.
			
			SDL_Flip(ecran);//on actualise l'ecran
			besoin_actualisation=0;
		}
		
		if((SDL_GetTicks()-temps_precedent)<(1000/FPS)) SDL_Delay((1000/FPS)-(SDL_GetTicks()-temps_precedent));//si le programme est allé trop vite par rapport au FPS : on force
		temps_precedent=SDL_GetTicks();//le programme a attendre un peu
	}
	return 0;
}


void dessiner_fractale(struct params_dessiner_fractale *params_dessiner_fractale)
{	

	int largeur_ecran=params_dessiner_fractale->largeur_ecran,//on recopie la structure dans des variables(enfin, on fait juste une recopie d'adresse :p )
	    hauteur_ecran=params_dessiner_fractale->hauteur_ecran,//cela n'a absolument aucun interet en soit, c'est juste pour avoir des noms de variables plus "friendly" et corrects
		
	debut_ecran_y=params_dessiner_fractale->debut_ecran_y,
	fin_ecran_y=params_dessiner_fractale->fin_ecran_y,
	
	*continuer=params_dessiner_fractale->continuer,
	
	x,y,i;//des compteurs de variable

	long double debut_x=params_dessiner_fractale->debut_x,
	fin_x=params_dessiner_fractale->fin_x,
	debut_y=params_dessiner_fractale->debut_y,
	fin_y=params_dessiner_fractale->fin_y,
	
	c_r,c_i,z_r,z_i,//les complexes c et z
	temp,//pour le calcul
	ratio;//le ratio nombre_iterations/nombre_iterations_max

	SDL_Surface *ecran=params_dessiner_fractale->ecran;
	
	//seules 2 variables sont des pointeurs communs à chaque thread : continuer et ecran. Ce sont les seules variables qui néssécitent une protection par mutex
	pthread_mutex_lock(&mutex);//on verrouille l'acces a la variable continuer

	for(x=0;x<largeur_ecran && *continuer;x++)
	{
		for(y=debut_ecran_y;y<fin_ecran_y;y++)
		{
			pthread_mutex_unlock(&mutex);//on dévérouille
	
			c_r= ((long double)x)/(((long double)largeur_ecran)/(fin_x-debut_x))+debut_x;//on dois calculer pour (-1;1) et (-2;1). Donc on se débrouille pour que 
			c_i= ((long double)y)/(((long double)hauteur_ecran)/(fin_y-debut_y))+debut_y;//quand x minimum(x=0) c_r=-2 et quand x maximum(x=largeur_ecran), c_r=1. Meme concept pour la partie imaginaire.
			if((c_r*c_r+c_i*c_i)>(2.0*2.0))
			{
				ratio=1.0/((long double)PRECISION_MAX);
			}
			else 
			{
				for(i=0,z_r=0.0,z_i=0.0;(z_i*z_i+z_r*z_r)<(2.0*2.0) && i<PRECISION_MAX && continuer; i++)//on vérifie, si, le nombre ne tend pas vers +8 (c'est a dire si |z|<2 ), avec
				{// une certaine précision. Cela a pour effet de donner a i une valeur plus ou moins grande
					temp=z_r;
					z_r=z_r*z_r - z_i*z_i + c_r;//z n+1=zn²+c. On calcule d'abord la partie réelle. Avec un peu de calcul, on remarque que Re(z n+1)= Re(z n)²-Im(z n)²+Re(c)
					z_i=2*z_i*temp + c_i;//et que Im(z n+1)  =  2*Re(z n)*Im(z n)+Im(c)
				}
				ratio=((long double)i)/((long double)PRECISION_MAX);
				
			}
			
			pthread_mutex_lock(&mutex);//on verouille l'acces pour l'affichage

			*((Uint32 *)ecran->pixels + y*ecran->w + x) = ((int)((ratio)*16777215.0));//on affiche le pixel dans une couleur proportionelle
			
		}	
	}
	
	pthread_mutex_unlock(&mutex);//on dévérouille pour éviter un death lock.
}
