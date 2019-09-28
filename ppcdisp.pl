#!/usr/bin/perl
# (c)TORO
#
	$dest = 'PPC_DISP.H';

	open(OUT, "> $dest");
	while (<DATA>) {
		chop;
		if ( $_ eq '' ){
			print OUT "\n";
			next;
		}
		$top = substr($_,0,1);
		if ( $top eq ';' ){ next; }
		if ( ($top eq '#') || ($top eq '/') ){
			print OUT "$_\n";
			next;
		}
		@tmp = split(/\t+/, $_);
		print OUT "#define @tmp[1]\t@tmp[0]\n";
		($list[$cnt][0],$list[$cnt][1],$list[$cnt][2],$list[$cnt][3]) = @tmp;
		$cnt++;
	}
	$max = $cnt - 1;
	print OUT "// max ID:$max";

# --------- �T�C�Y
	print OUT "\n\n//�e�T�C�Y\n";
	for ( 0 .. ($cnt - 1) ){
		print OUT "#define $list[$_][1]_SIZE $list[$_][2]\n";
	}
# --------- ID���Ƀ\�[�g
	for ( 0 .. ($cnt - 1) ){
		push(@sortkey,$list[$_][0]);
	}
	@list = @list[sort {$sortkey[$a] <=> $sortkey[$b]} 0..($cnt - 1)];
# --------- �X�L�b�v�e�[�u��
	print OUT "\n//�X�L�b�v�e�[�u��\n#define DE_SKIPTABLE	";
	for ( 0 .. ($cnt - 1) ){
		print OUT "$list[$_][2]";
		if ( $_ != ($cnt - 1) ){print OUT ",";}
	}
# --------- �����e�[�u��
	print OUT "\n//�����e�[�u��\n#define DE_ATTRTABLE	";
	for ( 0 .. ($cnt - 1) ){
		print OUT $list[$_][3];
		if ( $_ != ($cnt - 1) ){print OUT ",";}
	}
# ---------
	print OUT "\n";
	close(OUT);

__END__
// PPc �\�������֘A(ppcdisp.pl�ɂ�鎩������)
// �e��`�ꗗ
// PPC_DISP ��`�p�萔

; ��1
#define DE_ATTR_STATIC	B0	// ��ʍ\���v�f�Ȃ�
#define DE_ATTR_ENTRY	B1	// �G���g�����
#define DE_ATTR_MARK	B2	// �}�[�N���
#define DE_ATTR_PAGE	B3	// �y�[�W���
#define DE_ATTR_DIR		B4	// �f�B���N�g�����
#define DE_ATTR_PATH	B5	// �f�B���N�g���p�X�s
#define DE_ATTR_ALL		0xff	// �f�B���N�g�����

#define DE_ATTR_WIDEV	B16	// �����ɍ��킹�đ��g�������K�v
#define DE_ATTR_WIDEW	B17	// ���g�ɍ��킹�Č����������K�v

#define DE_ENABLE_FIXLENGTH	T("CFHRSVfns") // ���g�ϒ��ɑΉ�

#define GetIcon2Len(iconsize) ((iconsize + 7) / 8) // �A�C�R��pix�𕶎��񒷂ɕϊ�

#define DE_FN_ALL_WIDTH	0xff // �t�@�C�������̒����̐���������
#define DE_FN_WITH_EXT	0xff // �g���q�̓t�@�C�����ɑ����ĕ`��
;ID	Macro��  size  ��1  �R�����g
// static �n
0	DE_END		0	1	����
38	DE_SKIP		1	1	���E���킹�p�̃_�~�[�R�[�h(UNICODE�ŗp)
1	DE_SPC		2	1	Sn	��				(BYTE:��)
2	DE_BLANK	2	1	sn	��				(BYTE:��)
14	DE_sepline	1	1	L	��؂��
50	DE_hline	2	1	Hn	������				(BYTE:��)
47	DE_NEWLINE	3	1	/	���s				(WORD:���̍s�ւ̃I�t�Z�b�g)
16	DE_itemname	-1	1	I"str"	�A�C�e����		(z string)
27	DE_string	-1	1	i"str"	������			(z string)
57	DE_ivalue	-1	1	vi"str" id�ʓ�����ϐ�	(z string)
48	DE_fcolor	5	1	O"color" �����F			(COLORREF:�F)
49	DE_bcolor	5	1	Ob"color" �w�i�F		(COLORREF:�F)
56	DE_fc_def	2	1	Odn �����F���`�ςݐF��	(BYTE:���)
58	DE_bc_def	2	1	Og �w�i�F���`�ςݐF��	(BYTE:���)
// �G���g���n
; �}�[�N/�A�C�R���\��
3	DE_MARK		1	6	M	*
36	DE_ICON		1	6	N	�A�C�R��
46	DE_ICON2	2	6	Nn	�A�C�R��			(BYTE:�傫��)
37	DE_IMAGE	3	6	nm,n	�摜			(BYTE:��,BYTE:��)
59	DE_CHECK	1	6	b	�`�F�b�N
60	DE_CHECKBOX	1	6	B	�`�F�b�N�{�b�N�X
; �G���g����
4	DE_LFN		3	2	Fm,n	LFN�t�@�C����	(�t�@�C������,�g���q��)
5	DE_SFN		3	2	fm,n	SFN�t�@�C����	(�t�@�C������,�g���q��)
52	DE_LFN_MUL	3	2	FMm,n	LFN�����s(�ŏI�s�ȊO)
53	DE_LFN_LMUL	3	2	FMm,n	LFN�����s(�ŏI�s)
62	DE_LFN_EXT	3	2	FEm,n	LFN�t�@�C����(�g���q�D��)
63	DE_SFN_EXT	3	2	fEm,n	SFN�t�@�C����(�g���q�D��)
; �t�@�C���T�C�Y
6	DE_SIZE1	1	2	Z	�t�@�C���T�C�Y1(7)
11	DE_SIZE2	2	2	zn	�t�@�C���T�C�Y2		(BYTE:��)
12	DE_SIZE3	2	2	Zn	�t�@�C���T�C�Y3		(BYTE:��)
44	DE_SIZE4	2	2	zKn	�t�@�C���T�C�Y4,k	(BYTE:��)
; ����
7	DE_TIME1	2	2	Tn	�X�V����			(BYTE:��)
13	DE_TIME2	-2	2	t"s"����(������)		(���,ASCIIZ)
; ����
8	DE_ATTR1	2	2	An	�\������			(BYTE:��)
; �R�����g
9	DE_MEMO		2	2	Cn	�R�����g			(BYTE:��)
39	DE_MEMOS	2	2	Cn	�R�����g(�X�L�b�v�L)(BYTE:��)
10	DE_MSKIP	1	2	c	�R�����g�X�L�b�v
61	DE_MEMOEX	3	2	u	�g���R�����g		(BYTE:��,BYTE:ID)
; �J����
54	DE_COLUMN	-3	2	U	�J�����g��			(BYTE:��,WORD:����memo,BYTE:���g�̑傫��,TCHARZ ����)
; �g��
55	DE_MODULE	16	2	X	Module�g��			(BYTE:��,BYTE:��,DWORD:�n�b�V�� BYTE[9]:������)

; ���擪�z�u (BYTE:�ŏ��l�ABYTE:�ő�l�ABYTE:�����Hoff)
34	DE_WIDEV	4	2	W	�ϒ�����
35	DE_WIDEW	4	2	w	�ϒ�����(���g�ˑ��^)
// �}�[�N
28	DE_MNUMS	1	4	mn	�}�[�N��
19	DE_MSIZE1	2	4	mSn	�}�[�N�T�C�Y�u,�v�Ȃ�(BYTE:��)
20	DE_MSIZE2	2	4	msn	�}�[�N�T�C�Y�u,�v����(BYTE:��)
45	DE_MSIZE3	2	4	mKn	�}�[�N�T�C�Y�u,�v����,k(BYTE:��)
// �y�[�W
29	DE_ALLPAGE	1	8	P	�S�y�[�W��
30	DE_NOWPAGE	1	8	p	���݃y�[�W
// �f�B���N�g�����
15	DE_FStype	1	16	Y	�t�@�C���V�X�e����	3���Œ�
42	DE_vlabel	2	16	V	�{�����[�����x��	(BYTE:��)
43	DE_path		2	32	R	���݂̃f�B���N�g��	(BYTE:��)
51	DE_pathmask	2	16	RM	�f�B���N�g���}�X�N	(BYTE:��)

17	DE_ENTRYSET	1	16	E	�G���g���� �\��/�S��
40	DE_ENTRYA0	1	16	E0	�G���g���� �S��
41	DE_ENTRYA1	1	16	E1	�G���g���� �S��(./..������)

18	DE_ENTRYV0	2	16	en	�G���g���� �\��		(BYTE:��)		S2
31	DE_ENTRYV1	1	16	e1	�G���g���� �\��(./..������)	S3
32	DE_ENTRYV2	1	16	e2	�G���g���� �\�� dir	S4
33	DE_ENTRYV3	1	16	e3	�G���g���� �\�� file	S5

21	DE_DFREE1	2	16	DF	�󂫗e�ʁu,�v�Ȃ�	(BYTE:��)
22	DE_DFREE2	2	16	Df	�󂫗e�ʁu,�v����	(BYTE:��)
23	DE_DUSE1	2	16	DU	�g�p�e�ʁu,�v�Ȃ�	(BYTE:��)
24	DE_DUSE2	2	16	Du	�g�p�e�ʁu,�v����	(BYTE:��)
25	DE_DTOTAL1	2	16	DT	���e�ʁu,�v�Ȃ�		(BYTE:��)
26	DE_DTOTAL2	2	16	Dt	���e�ʁu,�v����		(BYTE:��)
