function! s:live(line, col)
  let ch = getline(a:line)[a:col - 1]
  return ch != ' ' && ch != '-' && ch != '_' && ch != ''
endfunction

function! s:vonNeumann(line, col)
  return s:live(a:line - 1, a:col) + s:live(a:line + 1, a:col) + s:live(a:line, a:col - 1) + s:live(a:line, a:col + 1)
endfunction

function! s:moore(line, col)
  return s:live(a:line - 1, a:col) + s:live(a:line + 1, a:col) + s:live(a:line, a:col - 1) + s:live(a:line, a:col + 1) + s:live(a:line - 1, a:col - 1) + s:live(a:line + 1, a:col - 1) + s:live(a:line - 1, a:col + 1) + s:live(a:line + 1, a:col + 1)
endfunction

function! gol#step()
  let height = line('$')
  let line = 1
  let updates = []
  while line <= height
    let width = col('$')
    let col = 1
    while col <= width
      let neighbours = s:moore(line, col)
      if s:live(line, col)
        if neighbours < 2 || neighbours > 3
          let updates += [[line, col, '-']]
        endif
      elseif neighbours == 3
        let updates += [[line, col, 'o']]
      endif
      let col += 1
    endwhile
    let line += 1
  endwhile
  for [line, col, c] in updates
    call cursor(line, col)
    execute 'normal r' . c
  endfor
  call cursor(1, 1)
endfunction

function! gol#init(width, height)
  %delete
  syntax match GolLive 'o'
  syntax match GolDead '-'
  highlight GolDead ctermfg=Black ctermbg=Black guibg=#101010 guifg=#101010
  highlight GolLive ctermfg=Gray ctermbg=White guibg=#f0f0f0 guifg=#f0f0f0
  execute 'normal ' . a:width . "i-\<esc>yy" . (a:height - 1) . 'P'
endfunction

function! gol#run(...)
  if a:0 > 0
    let n = a:1
    for i in range(n)
      call gol#step()
      redraw
    endfor
  else
    while 1
      call gol#step()
      redraw
    endwhile
  endif
endfunction
