#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#pragma warning(disable:4996) //rimuovere questo commento per ignorare errori di input non sicuro

//costanti di acquisizione
#define TR_LEN 256 //dimensione buffer di acquisizione
#define TR_CHUNK 10 //dimensione iniziale di allocazione vettore transizioni

//costanti di transizione
#define BLANK '_'

#define LEFT_MOV 'L'
#define RIGHT_MOV 'R'
//#define STOP_MOV 'S'

//costanti di simulazione
#define SIM_SUCCESS '1'
#define SIM_FAIL '0'
#define SIM_UNDEFINED 'U'

#define TRUE 1
#define FALSE 0

typedef struct struct_attr { //attributi di ogni transizione
	unsigned long int st_in; //stato di partenza
	char ch_read; //carattere letto dal nastro
	char ch_write; //carattere scritto sul nastro
	char mov; //carattere di controllo movimento testina: {R, L, S}
	unsigned long int st_fin; //stato di arrivo
} t_attr;

typedef struct struct_trans { //transizione
	t_attr attr; //attributi transizione
	int acc; //flag per indicare che attr.st_fin è di accettazione
	struct struct_trans *next;
} list_t;

typedef struct struct_execution_queue { //coda di esecuzione delle transizioni (fifo)
	list_t *nexttr; //puntatore alla prossima transizione da eseguire

	char *runstr; //testa del nastro di esecuzione corrente
	unsigned long int rlen; //lunghezza del nastro di esecuzione corrente
	unsigned long int marker; //posizione testina sul nastro corrente

	unsigned long int excount; //contatore transizioni ancora da eseguire

	struct struct_execution_queue *next;
}list_ex;

void free_ex_list(list_ex*);
void free_t_array(list_t**, unsigned long int);
void free_t_list(list_t*);
char *duplicate(char*, unsigned long int, unsigned long int*, unsigned long int, unsigned long int*);
char *transit(char, char, char*, unsigned long int*, unsigned long int*);
char *inschrunstr(char*, unsigned long int*, char);
char simulate(char *, unsigned long int, list_t**, unsigned long int, unsigned long int);
t_attr strtoattr(char[]);
list_ex *push(list_ex*, list_t*, char*, unsigned long int, unsigned long int, unsigned long int);
list_ex *pop(list_ex*, list_t**, char**, unsigned long int*, unsigned long int*, unsigned long int*);
list_t **instr(list_t**, unsigned long int*, t_attr);
list_t **putacc(list_t**, unsigned long int, unsigned long int);
list_t **talloc(list_t**, unsigned long int, unsigned long int);

void free_ex_list(list_ex *head) { //dealloca una lista di tipo: list_ex
	list_ex *tmp;

	while (head != NULL) {
		tmp = head;
		free(tmp->runstr);
		head = head->next;
		free(tmp);
	}
}

void free_t_list(list_t *head) {//dealloca una lista di tipo: list_t
	list_t *tmp;

	while (head != NULL) {
		tmp = head;
		head = head->next;
		free(tmp);
	}
}

void free_t_array(list_t **head, unsigned long int len) { //dealloca un array di tipo: list_t
	for (unsigned long int i = 0; i < len; ++i)
		free_t_list(head[i]);
	free(head);
}

t_attr strtoattr(char str[]) { //da stringa a t_attr
	t_attr attr;
	sscanf(str, "%ld %c %c %c %ld", &attr.st_in, &attr.ch_read, &attr.ch_write, &attr.mov, &attr.st_fin);
	return attr;
}

list_t **talloc(list_t **head, unsigned long int start, unsigned long int newlen) { //alloca e inizializza (newlen - start) elementi dell'array list_t
	head = (list_t **)realloc(head, newlen * sizeof(list_t));

	for (unsigned long int i = start; i < newlen; ++i)
		head[i] = NULL;
	return head;
}

char *inschrunstr(char *head, unsigned long int *len, char c) { //carica un carattere in coda al nastro
	++*len;

	head = (char *)realloc(head, *len * sizeof(char));
	head[*len - 1] = c;

	return head;
}

list_t **instr(list_t **head, unsigned long int *len, t_attr attr) { //inserisce transizione nel vettore delle transizioni
	list_t *elem;
	if (attr.st_in < *len) { //il vettore ha la posizione attr.st_in
		elem = (list_t *)malloc(sizeof(list_t));
		elem->acc = FALSE;
		elem->attr = attr;

		elem->next = head[attr.st_in];
		head[attr.st_in] = elem;
	}
	else { //il vettore non ha la posizione attr.st_in: devo allocare nuova memoria e poi inserire il nuovo valore
		head = talloc(head, *len, *len + TR_CHUNK); //alloco e inizializzo altri TR_CHUNK elementi
		*len = *len + TR_CHUNK; //aggiorno la nuova lunghezza
		return (head = instr(head, len, attr)); //inserisco attr nella posizione corretta
	}
	return head;
}

list_t **putacc(list_t **thead, unsigned long int len, unsigned long int acc) { //aggiorna il flag di accettazione per ogni transizione
	list_t *ttmphead;

	for (unsigned long int i = 0; i < len; ++i)
		for (ttmphead = thead[i]; ttmphead != NULL; ttmphead = ttmphead->next)
			if (ttmphead->attr.st_fin == acc) {
				ttmphead->acc = TRUE;
				return thead;
			}
	return thead;
}

list_ex *push(list_ex *exhead, list_t *nexttr, char *rhead, unsigned long int rlen, unsigned long int marker, unsigned long int count) { //inserisce un elemento list_ex in coda alla coda di esecuzione
	list_ex *tmp, *tmphead;

	tmp = (list_ex *)malloc(sizeof(list_ex));
	tmp->nexttr = nexttr;
	tmp->runstr = duplicate(rhead, rlen, &tmp->rlen, marker, &tmp->marker);
	tmp->excount = count;

	tmp->next = NULL;

	if (exhead == NULL)
		exhead = tmp;
	else {
		for (tmphead = exhead; tmphead->next != NULL; tmphead = tmphead->next);
		tmphead->next = tmp;
	}
	return exhead;
}

list_ex *pop(list_ex *exhead, list_t **tr, char **rhead, unsigned long int *rlen, unsigned long int *marker, unsigned long int *count) { //estrae e rimuove dalla lista il primo elemento
	list_ex *tmp;
	tmp = exhead;
	exhead = exhead->next;

	*tr = tmp->nexttr;
	*rhead = duplicate(tmp->runstr, tmp->rlen, rlen, tmp->marker, marker);
	*count = tmp->excount;

	tmp->next = NULL;
	free_ex_list(tmp);
	return exhead;
}

char *transit(char ch_write, char mov, char *rhead, unsigned long int *rlen, unsigned long int *marker) { //simula una transizione della MT

	rhead[*marker] = ch_write;

	switch (mov) {
	case LEFT_MOV: //sposto la testina a sinistra di una posizione
		if (*marker > 0)
			--*marker;
		else { //marker = 0
			++*rlen;
			rhead = (char *)realloc(rhead, *rlen * sizeof(char));

			//shift a destra di tutti i valori
			for (unsigned long int i = *rlen - 1; i > 0; i--)
				rhead[i] = rhead[i - 1];
			rhead[*marker] = BLANK;
		}
		break;
	case RIGHT_MOV: //sposta la testina a destra di una posizione
		++*marker;

		if (*marker == *rlen) {
			++*rlen;
			rhead = (char *)realloc(rhead, *rlen * sizeof(char));
			rhead[*marker] = BLANK;
		}
		break;
	}
	return rhead;
}

char *duplicate(char *rhead, unsigned long int rlen, unsigned long int *newrlen, unsigned long int marker, unsigned long int *newmarker) { //duplica una lista e il relativo marker
	char *newrhead;

	*newrlen = rlen; //aggiorno la lunghezza del nuovo nastro
	*newmarker = marker; //aggiorno il marker nella nuova lista

	newrhead = NULL;
	newrhead = (char *)realloc(newrhead, *newrlen * sizeof(char)); //creo il nuovo nastro

	//copio i valori del vacchio nastro al nuovo nastro
	for (unsigned long int i = 0; i < *newrlen; ++i)
		newrhead[i] = rhead[i];
	return newrhead;
}

char simulate(char *rhead, unsigned long int rlen, list_t **thead, unsigned long int tlen, unsigned long int excount) { //esegue tutte le possibili transizioni su una stringa rhead
	list_ex *exhead; //coda di esecuzione (FIFO)
	list_t *tcurr; //transizione corrente
	list_t *tnext; //prossima transizione da eseguire
	char *rcurrhead; //testa del nastro corrente
	unsigned long int rcurrlen; //lunghezza nastro corrente
	unsigned long int count; //backup contatore istruzioni rimanenti (della transizione corrente)
	unsigned long int marker; //backup poosizione testina (della transizione corrente)

	//flags
	int u_flag = FALSE; //presenza di transizioni con esito U: TRUE: almeno una transizione ha dato esito U, FALSE: nessuna transizione ha dato esito U
	int pop_flag = FALSE; //indica la necessità di eseguire pop(): TRUE: devo eseguire pop() (una transizione non ha più transizioni successive da eseguire), FALSE: non devo eseguire pop() (una transizione ha ancora delle transizioni successive da eseguire)
	int first_ex_trans = FALSE; //indica la prima transizione eseguibile: TRUE: ho già incontrato la prima transizione eseguibile (quella attuale non è la prima), FALSE: non ho ancora incontrato una transizione eseguibile (quella attuale è la prima)
	int continue_flag = FALSE; //indica quando devo terminare il while più esterno: TRUE: devo continuare (per evitare che exhead è vuota ma ho ancora istruzioni da eseguire), FALSE: devo terminare (exhead è vuota e non ho più transizioni da eseguire)

	exhead = NULL;
	rcurrhead = NULL;

	//carico la coda di esecuzione con le transizioni che partono da zero
	for (tcurr = NULL, tnext = thead[0]; tnext != NULL; tnext = tnext->next)
		if (tnext->attr.ch_read == rhead[0]) { //controllo che possa eseguire tale transizione
			switch (first_ex_trans) {
			case FALSE: //tnext è la prima transizione eseguibile che incontro: la carico nelle variabili di lavoro
				first_ex_trans = TRUE;
				continue_flag = TRUE;

				tcurr = tnext;
				rcurrhead = duplicate(rhead, rlen, &rcurrlen, 0, &marker);
				count = excount;
				break;
			case TRUE: //tnext non è la prima tranisizione eseguibile che incontro: la devo salvare nella coda di esecuzione
				exhead = push(exhead, tnext, rhead, rlen, 0, excount);
				break;
			}
		}

	//computo tutte le possibili esecuzioni che partono da zero
	while (continue_flag == TRUE) { //finchè ho transizioni da esseguire
		if (pop_flag == TRUE) {
			if (exhead != NULL) {
				free(rcurrhead);
				exhead = pop(exhead, &tcurr, &rcurrhead, &rcurrlen, &marker, &count); //estraggo la prossima transizione da eseguire dalla coda di esecuzione
			}
			else //ogni transizione ha eseguito tutte le sue successive e exlist è vuota
				continue_flag = FALSE;
		}

		if (continue_flag == TRUE) {
			pop_flag = TRUE; //se l'istruzione attuale non ha successive oppure se tutte le successive non possono essere eseguite, questo flag rimane a TRUE: devo fare la pop()
			--count;
			if (count > 0) { //posso ancora eseguire transizioni
				rcurrhead = transit(tcurr->attr.ch_write, tcurr->attr.mov, rcurrhead, &rcurrlen, &marker); //eseguo la transizione tcurr

				if (tcurr->attr.st_fin < tlen) { //esiste questa posizione nel vettore: tcurr ha una sucessiva
					tnext = thead[tcurr->attr.st_fin]; //recupero la prossima transizione dal vettore delle transizioni

					if (tnext != NULL) { //ho almeno una transizione da eseguire (se tnext = NULL, tcurr non ha transizioni successive)
						for (first_ex_trans = FALSE; tnext != NULL; tnext = tnext->next) { //per tutte le possibili transizioni successive
							if (rcurrhead[marker] == tnext->attr.ch_read) { //controllo se la transizione può essere eseguita
								if (tnext->acc == TRUE) { //la transizione successiva mi porterebbe in uno stato finale
									free(rcurrhead);
									free_ex_list(exhead);
									return SIM_SUCCESS;
								}
								//controllo loop infiniti a sinistra (con marker = 0) e a destra (con marker = rcurrlen-1)
								if ((tnext->attr.st_in == tnext->attr.st_fin && tnext->attr.ch_read == BLANK)
									&& ((marker == 0 && tnext->attr.mov == LEFT_MOV) || (marker == rcurrlen - 1 && tnext->attr.mov == RIGHT_MOV)))
									u_flag = TRUE;
								else { //DA QUI IN AVANTI NON HO LOOP INFINITI E LA PROSSIMA TRANSIZIONE NON MI PORTEREBBE IN UNO STATO FINALE
									switch (first_ex_trans) {
									case FALSE: //la prossima transizione (tnext) può essere eseguita ed è la prima
										first_ex_trans = TRUE; //ho trovato la prima transizione che può essere eseguita
										pop_flag = FALSE; //ora che ho una transizione sicuramente da eseguire alla prossima iterazione, sono sicuro che non devo fare pop()

										//aggiorno i parametri che varranno usati da transit():
										//count, marker, rcurrhead, rcurrlen: non devono essere modificati
										tcurr = tnext; //aggiorno la prossima transizione da eseguire
										break;
									case TRUE: //la prossima transizione (tnext) può essere eseguita, ma non è la prima
										exhead = push(exhead, tnext, rcurrhead, rcurrlen, marker, count); //aggiungo la nuova transizione alla coda di esecuzione (in coda)
										break;
									}
								}
							} //else: la transizione tnext non può essere eseguita
						}
					} //else: non ho transizini successive da eseguire
				} //else: non esiste questa posizione nel vettore: tcurr non ha una successiva ma lo stato finale non è di accettazione (avrei ritornato 1 al  passo precedente)
			}
			else //ho raggiunto il massimo di transizioni da eseguire
				u_flag = TRUE;
		}
	}
	//ho finito le transizioni da eseguire
	free(rcurrhead);
	free_ex_list(exhead);

	if (u_flag == TRUE)
		return SIM_UNDEFINED;
	return SIM_FAIL;
}

int main() {
	char line[TR_LEN + 1];
	int ch;
	unsigned long int runlen, trlen, max, acc;
	list_t **trhead;
	char *runstr;

	fgets(line, TR_LEN + 1, stdin); //salto 'tr'

	//leggo i tr
	trlen = TR_CHUNK;
	trhead = NULL;
	trhead = talloc(trhead, 0, trlen); //alloco e inizializzo le prime TR_CHUNK posizioni

	fgets(line, TR_LEN + 1, stdin);
	while (strcmp(line, "acc\n") != 0) {
		trhead = instr(trhead, &trlen, strtoattr(line));
		fgets(line, TR_LEN + 1, stdin);
	}

	//leggo gli acc
	scanf("%d", &acc);

	do
		trhead = putacc(trhead, trlen, acc);
	while (scanf("%d", &acc) != 0); //se scanf ritorna 0 ho letto 'max'

	fgets(line, TR_LEN + 1, stdin); //salto 'max'

	scanf("%d\n", &max); //leggo max e vado a capo

	fgets(line, TR_LEN + 1, stdin); //salto 'run'

	//simulo tutte le possibili transizioni su tutte le stringhe run (una per volta)
	runlen = 0;
	while (EOF != (ch = getchar())) {
		runstr = NULL;
		do //leggo un run
			runstr = inschrunstr(runstr, &runlen, ch);
		while ('\n' != (ch = getchar()) && EOF != ch);

		if (runstr[0] != '\n') //computo solo stringhe non vuote (le stringhe che cominciano con '\n' sono vuote)
			printf("%c\n", simulate(runstr, runlen, trhead, trlen, max));
		free(runstr);
		runlen = 0;
	}

	free_t_array(trhead, trlen);
	return 0;
}